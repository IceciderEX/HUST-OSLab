#ifndef _MEMLAYOUT_H
#define _MEMLAYOUT_H
#include "riscv.h"

// RISC-V machine places its physical memory above DRAM_BASE
#define DRAM_BASE 0x80000000

// the beginning virtual address of PKE kernel
#define KERN_BASE 0x80000000

// default stack size
#define STACK_SIZE 4096

// virtual address of stack top of user process
#define USER_STACK_TOP 0x7ffff000

// start virtual address (4MB) of our simple heap. added @lab2_2
#define USER_FREE_ADDRESS_START 0x00000000 + PGSIZE * 1024

// PKE_MAX_ALLOWABLE_RAM (128MB)
#define PKE_MAX_ALLOWABLE_RAM 128 * 1024 * 1024 

// the ending physical address that PKE observes. added @lab2_1
#define PHYS_TOP (DRAM_BASE + PKE_MAX_ALLOWABLE_RAM)

#endif
