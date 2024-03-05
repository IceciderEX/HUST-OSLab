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
#include "kernel/sync_utils.h"
#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  int hartid = read_tp();
  sprint("hartid = %d: %s", hartid, buf);
  return 0;
}

// sync shutdown
static volatile int shutdown_counter = 0;
//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  int hartid = read_tp();
  sprint("hartid = %d: User exit with code:%d.\n", hartid, code);
  // 在单核环境下，一个核的用户进程调用exit退出时，会立即关闭模拟器；
  // 而在多核环境下，这会强使其他核也停止工作。正确的退出方式是，同步等到所有核执行完毕之后再关闭模拟器。本次实验中，你应当让CPU0负责关闭模拟器。
  sync_barrier(&shutdown_counter, NCPU);
  if(hartid == 0){
    sprint("hartid = %d: shutdown with code:%d.\n", hartid, code);
    shutdown(code);
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
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
