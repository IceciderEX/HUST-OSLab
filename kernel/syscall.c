/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"

#include "spike_interface/spike_utils.h"

extern elf_symbol all_symbols[100];
extern char symbol_names[100][30];
extern int symbol_count;

int get_funcname(uint64 ra){
  //sprint("%ld\n", ra);
  for(int i = 0;i < symbol_count;++i){
    //sprint("%ld\n", all_symbols[i].value);
    // 地址在调用的某个函数的栈帧范围中
    if(ra >= all_symbols[i].value && ra < all_symbols[i].value + all_symbols[i].size){
      sprint("%s\n", symbol_names[i]);
      if(strcmp(symbol_names[i] , "main") == 0) { // 递归最深到main函数
        return -1;
      }
      return 0;
    }
  }
  return 0;
}

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

ssize_t sys_user_print_backtrace(int layer){
  // call elf_print_backtrace in elf.c
  uint64 sp = current->trapframe->regs.sp + 32; // 当前sp
  uint64 ra = sp + 8; // 返回地址
  //sprint("Enter user printtrace!, layer:%d\n", layer);
  int i = 0;
  for(i = 0;i < layer;++i){
    int res = get_funcname(*(uint64*)ra); // 求得函数在栈帧中的地址
    if(res == -1){ // main
      return 0;
    }
    ra += 16;
  }
  return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    case SYS_user_print_backtrace:
      return sys_user_print_backtrace(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
