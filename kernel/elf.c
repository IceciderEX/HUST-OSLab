/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"
#include "string.h"
#include "riscv.h"
#include "vmm.h"
#include "pmm.h"
#include "vfs.h"
#include "spike_interface/spike_utils.h"
#include "hostfs.h"
#include "memlayout.h"

typedef struct elf_info_t {
  struct file *f;
  process *p;
} elf_info;

//
// the implementation of allocater. allocates memory space for later segment loading.
// this allocater is heavily modified @lab2_1, where we do NOT work in bare mode.
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
  elf_info *msg = (elf_info *)ctx->info;
  // we assume that size of proram segment is smaller than a page.
  kassert(size < PGSIZE);
  void *pa = alloc_page();
  if (pa == 0) panic("uvmalloc mem alloc falied\n");

  memset((void *)pa, 0, PGSIZE);
  user_vm_map((pagetable_t)msg->p->pagetable, elf_va, PGSIZE, (uint64)pa,
         prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));

  return pa;
}

//
// actual file reading, using the vfs file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
  elf_info *msg = (elf_info *)ctx->info;
  vfs_lseek(msg->f, offset, SEEK_SET);
  return vfs_read(msg->f, dest, nb);
}

//
// init elf_ctx, a data structure that loads the elf.
//
elf_status elf_init(elf_ctx *ctx, void *info) {
  ctx->info = info;

  // load the elf header
  if (elf_fpread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0) != sizeof(ctx->ehdr)) return EL_EIO;

  // check the signature (magic value) of the elf
  if (ctx->ehdr.magic != ELF_MAGIC) return EL_NOTELF;

  return EL_OK;
}

//
// load the elf segments to memory regions.
//
elf_status elf_load(elf_ctx *ctx) {
  // elf_prog_header structure is defined in kernel/elf.h
  elf_prog_header ph_addr;
  int i, off;

  // traverse the elf program segment headers
  for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD) continue;
    if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

    // allocate memory block before elf loading
    void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);

    // actual loading
    if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz)
      return EL_EIO;

    // record the vm region in proc->mapped_info. added @lab3_1
    int j;
    for( j=0; j<PGSIZE/sizeof(mapped_region); j++ ) //seek the last mapped region
      if( (process*)(((elf_info*)(ctx->info))->p)->mapped_info[j].va == 0x0 ) break;

    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].va = ph_addr.vaddr;
    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].npages = 1;

    // SEGMENT_READABLE, SEGMENT_EXECUTABLE, SEGMENT_WRITABLE are defined in kernel/elf.h
    if( ph_addr.flags == (SEGMENT_READABLE|SEGMENT_EXECUTABLE) ){
      ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type = CODE_SEGMENT;
      sprint( "CODE_SEGMENT added at mapped info offset:%d\n", j );
    }else if ( ph_addr.flags == (SEGMENT_READABLE|SEGMENT_WRITABLE) ){
      ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type = DATA_SEGMENT;
      sprint( "DATA_SEGMENT added at mapped info offset:%d\n", j );
    }else
      panic( "unknown program segment encountered, segment flag:%d.\n", ph_addr.flags );

    ((process*)(((elf_info*)(ctx->info))->p))->total_mapped_region ++;
  }

  return EL_OK;
}

//
// replace the elf segments to memory regions by exec func added@lab4_c2
//
elf_status elf_reload(elf_ctx *ctx) {
  // elf_prog_header structure is defined in kernel/elf.h
  elf_prog_header ph_addr;
  int i, off;

  // traverse the elf program segment headers
  for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD) continue;
    if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

    // exec会根据读入的可执行文件将原进程的数据段、代码段和堆栈段替换。
    // if there is no data segment, create a new one
    if(ph_addr.flags == (SEGMENT_READABLE | SEGMENT_EXECUTABLE)){
        for(int j = 0;j < PGSIZE / sizeof(mapped_region); j++){
            if(((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type == CODE_SEGMENT){
                sprint("CODE_SEGMENT added at mapped info offset:%d\n", j);
                user_vm_unmap((pagetable_t)(((elf_info*)(ctx->info))->p)->pagetable, ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].va, 
                                PGSIZE, 1);
                // allocate memory block before elf loading
                void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);
                // actual loading
                if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz) return EL_EIO;                
                ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].va = ph_addr.vaddr;
                ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].npages = 1;
                ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type = CODE_SEGMENT;
                break;
            }
        }
    }
    else if(ph_addr.flags == (SEGMENT_READABLE | SEGMENT_WRITABLE)){
        // if there is a data segment
        int flag = 0;
        for(int j = 0;j < PGSIZE / sizeof(mapped_region); j++){
            if(((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type == DATA_SEGMENT){
                sprint("DATA_SEGMENT added at mapped info offset:%d\n", j);
                user_vm_unmap((pagetable_t)(((elf_info*)(ctx->info))->p)->pagetable, ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].va, 
                                PGSIZE, 1);
                // allocate memory block before elf loading
                void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);
                // actual loading
                if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz) return EL_EIO;  
                ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].va = ph_addr.vaddr;
                ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].npages = 1;
                ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[j].seg_type = DATA_SEGMENT;
                flag = 1;
                break;
            }
        }
        if(flag == 0){
            // allocate memory block before elf loading
            void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);
            // actual loading
            if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz) return EL_EIO;

            for(int k = 0;k < PGSIZE / sizeof(mapped_region); k++){
                if((process*)(((elf_info*)(ctx->info))->p)->mapped_info[k].va == 0x0){
                    sprint("DATA_SEGMENT added at mapped info offset:%d\n", k);
                    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[k].va = ph_addr.vaddr;
                    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[k].npages = 1;
                    ((process*)(((elf_info*)(ctx->info))->p))->mapped_info[k].seg_type = DATA_SEGMENT;
                    ((process*)(((elf_info*)(ctx->info))->p))->total_mapped_region++;
                    break;
                }
            }
        }
    }
  }
  return EL_OK;
}

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p, char *filename) {
  sprint("Application: %s\n", filename);

  //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;

  info.f = vfs_open(filename, O_RDONLY);
  info.p = p;
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // close the vfs file
  vfs_close( info.f );

  sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

// 
void exec_elf_read(const char* pathname, process* p, char* para){
    //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;
  
  // sprint("exec file path: %s\n", pathname);
  // open file
  info.f = vfs_open(pathname, O_RDONLY);
  info.p = p;
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_reload(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // 完成参数加载
  // Set up argc and argv on the user stack
  uint64_t stack_top = USER_STACK_TOP; // Assuming USER_STACK_TOP is the top of the user stack
  uint64_t argc = 1; // Initially set to 1 for the program name
  //uint64_t arg_len = strlen(para) + 1; // Include null terminator
  //uint64_t argv_addr = stack_top - arg_len;
  stack_top -= sizeof(uint64_t);
  stack_top -= sizeof(uint64_t);
  memcpy(user_va_to_pa((pagetable_t)p->pagetable, (void*)stack_top), &argc, sizeof(uint64_t));

  // Adjust stack for argv
  stack_top -= sizeof(uint64_t); // Reserve space for the null pointer
  uint64 addr = (uint64)para;
  memcpy(user_va_to_pa((pagetable_t)p->pagetable, (void*)stack_top), &addr, sizeof(uint64_t));
  sprint("%x\n", user_va_to_pa((pagetable_t)p->pagetable, (void*)stack_top));
  sprint("%x\n", para);

  // Adjust process's stack pointer
  p->trapframe->regs.sp = stack_top;
  p->trapframe->regs.a1 = stack_top;
  
  // close the file
  vfs_close(info.f);

  sprint("Application program entry point (virtual address): 0x%lx\n", current->trapframe->epc);
  return;
}
