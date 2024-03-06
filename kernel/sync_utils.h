#ifndef _SYNC_UTILS_H_
#define _SYNC_UTILS_H_

#include "util/types.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"


typedef struct spinlock{
  uint64 locked; // whether be locked 

  char* name;
  uint64 hartid; // the hart holds this lock
}lock;

void acquire_lock(lock* lock);
void release_lock(lock* lock);

static inline void sync_barrier(volatile int *counter, int all) {

  int local;

  asm volatile("amoadd.w %0, %2, (%1)\n"
               : "=r"(local)
               : "r"(counter), "r"(1)
               : "memory");

  if (local + 1 < all) {
    do {
      asm volatile("lw %0, (%1)\n" : "=r"(local) : "r"(counter) : "memory");
    } while (local < all);
  }
}

#endif