/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"
#include "string.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"

elf_symbol all_symbols[100]; // 用于存储所有符号的elf表
char symbol_names[100][30]; // 用于存储所有符号的名字，用于打印函数名
int symbol_count; // 符号个数的计数

typedef struct elf_info_t {
  spike_file_t *f;
  process *p;
} elf_info;

//
// the implementation of allocater. allocates memory space for later segment loading
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
  // directly returns the virtual address as we are in the Bare mode in lab1_x
  return (void *)elf_va;
}

//
// actual file reading, using the spike file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
  elf_info *msg = (elf_info *)ctx->info;
  // call spike file utility to load the content of elf file into memory.
  // spike_file_pread will read the elf file (msg->f) from offset to memory (indicated by
  // *dest) for nb bytes.
  return spike_file_pread(msg->f, dest, nb, offset);
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
// load the elf segments to memory regions as we are in Bare mode in lab1
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
  }

  return EL_OK;
}

// 从elf header中获取所有的函数名称，储存到symbol_names
elf_status elf_load_funcname(elf_ctx* ctx){
  //sprint("Entering loading funcname!\n");
  elf_section_header str_header;
  elf_section_header symbol_header;
  elf_section_header shstr_header;
  elf_section_header header;

    // 先读取shstrtab表项的内容
  elf_fpread(ctx, (void *)&shstr_header, sizeof(shstr_header), ctx->ehdr.shoff + sizeof(shstr_header) * ctx->ehdr.shstrndx);
  char shstr_sec[shstr_header.size];
  elf_fpread(ctx, &shstr_sec, shstr_header.size, shstr_header.offset);
  uint16 shstrlab_offset = ctx->ehdr.shstrndx; // shstrlab在节头表中的索引
  //sprint("%d %d\n", shstr_header.size, shstr_header.offset);
  // 遍历每一个节头表，找到字符串表与符号表的位置
  for(int i = 0, offset = ctx->ehdr.shoff;i < ctx->ehdr.shnum;++i, offset += sizeof(elf_section_header)){
    // 读取此节头表的表项header
    if (elf_fpread(ctx, (void *)&header, sizeof(header), offset) != sizeof(header)) return EL_EIO;

    uint32 type = header.type;
    //sprint("%s\n",shstr_sec + header.name);
    if(type == SHT_STRTAB) { // 字符串表
      if(strcmp(shstr_sec + header.name, ".strtab") == 0){ // 为".strlab"表，可区分".shstrlab"
        str_header = header;
      }
    }
    else if(type == SHT_SYMTAB){ // 符号表
      symbol_header = header;
    }
  }

  uint64 symbol_num = symbol_header.size / sizeof(elf_symbol); // 符号总数

  //sprint("%ld\n", symbol_num); 24
  // 在符号表中获取各个函数的名称
  for(int i = 0, offset = symbol_header.offset;i < symbol_num;++i, offset += sizeof(elf_symbol)){
    elf_symbol symbol;
    // 读取符号表表项
    if (elf_fpread(ctx, (void *)&symbol, sizeof(symbol), offset) != sizeof(symbol)) return EL_EIO;
    
    //sprint("%x\n", symbol.info);
    if((symbol.info & 0xf) == STT_FUNC){ // 低四位值为STT_FUNC，是函数
      char func_name[30];
      uint64 func_offset = str_header.offset + symbol.name; // 计算在文件中的偏移
      elf_fpread(ctx, (void*)&func_name, sizeof(func_name), func_offset);
      //sprint("func_name: %s\n", func_name);
      all_symbols[symbol_count++] = symbol; // 存储到数组中
      strcpy(symbol_names[symbol_count - 1], func_name);
    } 
  }

  // for(int i = 0;i < symbol_count;++i){
  //   sprint("%s\n", symbol_names[i]);
  // }
  return EL_OK;
}

typedef union {
  uint64 buf[MAX_CMDLINE_ARGS];
  char *argv[MAX_CMDLINE_ARGS];
} arg_buf;

//
// returns the number (should be 1) of string(s) after PKE kernel in command line.
// and store the string(s) in arg_bug_msg.
//
static size_t parse_args(arg_buf *arg_bug_msg) {
  // HTIFSYS_getmainvars frontend call reads command arguments to (input) *arg_bug_msg
  long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
      sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
  kassert(r == 0);

  size_t pk_argc = arg_bug_msg->buf[0];
  uint64 *pk_argv = &arg_bug_msg->buf[1];

  int arg = 1;  // skip the PKE OS kernel string, leave behind only the application name
  for (size_t i = 0; arg + i < pk_argc; i++)
    arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

  //returns the number of strings after PKE kernel in command line
  return pk_argc - arg;
}

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p) {
  arg_buf arg_bug_msg;

  // retrieve command line arguements
  size_t argc = parse_args(&arg_bug_msg);
  if (!argc) panic("You need to specify the application program!\n");

  sprint("Application: %s\n", arg_bug_msg.argv[0]);

  //elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
  elf_ctx elfloader;
  // elf_info is defined above, used to tie the elf file and its corresponding process.
  elf_info info;

  info.f = spike_file_open(arg_bug_msg.argv[0], O_RDONLY, 0);
  info.p = p;
  // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
  if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

  // init elfloader context. elf_init() is defined above.
  if (elf_init(&elfloader, &info) != EL_OK)
    panic("fail to init elfloader.\n");

  // load elf. elf_load() is defined above.
  if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

  elf_status res = elf_load_funcname(&elfloader); // 获取各函数名称
  if(res == EL_EIO){
    panic("fail to load funcname.\n");
  }

  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;

  // close the host spike file
  spike_file_close( info.f );

  sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}
