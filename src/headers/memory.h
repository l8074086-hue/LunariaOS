#ifndef MEM_H
#define MEM_H

#include <stdint.h>

void memory_init(void);
void paging_init(void);

uint32_t memory_total_kb(void);
uint32_t memory_free_frames(void);
uint32_t memory_total_frames(void);

/* --- per-process address spaces ---------------------------------------
   The kernel half of every address space is the same identity map, so a
   process directory is just a copy of the kernel directory with its own
   page directory entries for the ring-3 window. Those entries start out
   empty and are filled in on demand by vm_fault(). */

uint32_t vm_create(void);            /* new, empty process directory      */
void     vm_attach(uint32_t dir);    /* load CR3, mark as current process  */
void     vm_detach(void);            /* back to the kernel dir, then free  */
void     vm_switch(uint32_t dir);    /* raw CR3 load                       */
void     vm_destroy(uint32_t dir);

int      vm_fault(uint32_t addr);    /* 1 = mapped, retry; 0 = fatal       */
void     vm_note_load(uint32_t size);/* record program size for vm_sbrk()  */
uint32_t vm_sbrk(uint32_t inc);
uint32_t vm_faults(void);

#endif
