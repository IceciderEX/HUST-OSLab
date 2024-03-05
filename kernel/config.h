#ifndef _CONFIG_H_
#define _CONFIG_H_

// we use TWO HART (cpus) in lab1_c3
#define NCPU 2

//interval of timer interrupt. added @lab1_3
#define TIMER_INTERVAL 1000000

#define DRAM_BASE 0x80000000

/* we use fixed physical (also logical) addresses for the stacks and trap frames as in
 Bare memory-mapping mode */

// add another cpu's stack @lab1_c3
// user stack top
#define USER_STACK_0 0x81100000
#define USER_STACK_1 0x81150000

// the stack used by PKE kernel when a syscall happens
#define USER_KSTACK_0 0x81200000
#define USER_KSTACK_1 0x81250000

// the trap frame used to assemble the user "process"
#define USER_TRAP_FRAME_0 0x81300000
#define USER_TRAP_FRAME_1 0x81350000

#endif
