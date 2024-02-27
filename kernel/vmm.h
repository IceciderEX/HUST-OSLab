#ifndef _VMM_H_
#define _VMM_H_

#include "riscv.h"
#include "process.h"

/* memory control block */
// (mcb + memory block malloc) is an unit when malloc
typedef struct memory_control_block
{
  uint64 start; // start addr(exclude mcb)
  uint64 avaiable; // 0: unavaiable(used), 1: avaiable(can malloc)
  uint64 size; 
  struct memory_control_block* next; // pointer, to next pcb
} mcb;


/* --- utility functions for virtual address mapping --- */
int map_pages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
// permission codes.
enum VMPermision {
  PROT_NONE = 0,
  PROT_READ = 1,
  PROT_WRITE = 2,
  PROT_EXEC = 4,
};

uint64 prot_to_type(int prot, int user);
pte_t *page_walk(pagetable_t pagetable, uint64 va, int alloc);
uint64 lookup_pa(pagetable_t pagetable, uint64 va);

/* --- kernel page table --- */
// pointer to kernel page directory
extern pagetable_t g_kernel_pagetable;

void kern_vm_map(pagetable_t page_dir, uint64 va, uint64 pa, uint64 sz, int perm);

// Initialize the kernel pagetable
void kern_vm_init(void);

/* --- user page table --- */
void *user_va_to_pa(pagetable_t page_dir, void *va);
void user_vm_map(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa, int perm);
void user_vm_unmap(pagetable_t page_dir, uint64 va, uint64 size, int free);

/* --- malloc and free func --- */
void init_mcb_list();
void my_map_for_nbytes(uint64 n);
uint64 my_malloc(uint64 n);
void my_free(uint64 va);

#endif