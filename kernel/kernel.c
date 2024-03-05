/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"

#include "spike_interface/spike_utils.h"

// process is a structure defined in kernel/process.h
// each hart have their own app
process user_app[NCPU];

//
// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
// 之前的实验在单核的基础上开展，且没有内存管理机制，所以user app使用到的一些内存地址是固定的（见kernel/config.h）。
// 即使实验基础代码在elf文件中为两个app指定了不同的起始地址，两个核同时加载并运行的它们时，其部分内存地址也会重叠并出错。你应当想办法分离两个app的内存。
//
void load_user_program(process *proc) {
  // USER_TRAP_FRAME is a physical address defined in kernel/config.h
  int hartid = read_tp();
  if(hartid == 0)
    proc->trapframe = (trapframe *)USER_TRAP_FRAME_0;
  else if(hartid == 1)
    proc->trapframe = (trapframe *)USER_TRAP_FRAME_1;
  memset(proc->trapframe, 0, sizeof(trapframe));
  // USER_KSTACK is also a physical address defined in kernel/config.h
  if(hartid == 0)
    proc->kstack = USER_KSTACK_0;
  else if(hartid == 1)
    proc->kstack = USER_KSTACK_1;
  if(hartid == 0)
    proc->trapframe->regs.sp = USER_STACK_0;
  else if(hartid == 1)  
    proc->trapframe->regs.sp = USER_STACK_1;
  // modify tp, or else proc always runs at hart 0
  proc->trapframe->regs.tp = hartid;
  // load_bincode_from_host_elf() is defined in kernel/elf.c
  load_bincode_from_host_elf(proc);
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
int s_start(void) {
  int hartid = read_tp();
  sprint("hartid = %d: Enter supervisor mode...\n", hartid); // hartid modify task
  // Note: we use direct (i.e., Bare mode) for memory mapping in lab1.
  // which means: Virtual Address = Physical Address
  // therefore, we need to set satp to be 0 for now. we will enable paging in lab2_x.
  // 
  // write_csr is a macro defined in kernel/riscv.h
  write_csr(satp, 0);

  // the application code (elf) is first loaded into memory, and then put into execution
  load_user_program(&user_app[hartid]);

  sprint("hartid = %d: Switch to user mode...\n", hartid); // hartid modify task
  // switch_to() is defined in kernel/process.c
  switch_to(&user_app[hartid]);

  // we should never reach here.
  return 0;
}
