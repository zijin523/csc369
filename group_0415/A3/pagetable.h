#ifndef __PAGETABLE_H__
#define __PAGETABLE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TRACE_64

#define PAGE_SHIFT      12     // number of bits 2^(PAGE_SHIFT) == PAGE_SIZE
#define PAGE_SIZE       4096 // Size of pagetable pages
#define PAGE_MASK       (~(PAGE_SIZE-1))
#define PG_VALID        (0x1) // Valid bit in pgd or pte, set if in memory
#define PG_DIRTY        (0x2) // Dirty bit in pgd or pte, set if modified
#define PG_REF          (0x4) // Reference bit, set if page has been referenced
#define PG_ONSWAP       (0x8) // Set if page has been evicted to swap
#define INVALID_SWAP    -1

#ifdef TRACE_64
// User-level virtual addresses on 64-bit Linux system are 36 bits in our traces
// and the page size is still 4096 (12 bits). 
// We split the remaining 24 bits evenly into top-level (page directory) index
// and second-level (page table) index, using 12 bits for each. 
#define PGDIR_SHIFT         24     // Leaves just top 12 bits of vaddr 
#define PTRS_PER_PGDIR    4096
#define PTRS_PER_PGTBL    4096

#else // TRACE_32
// User-level virtual addresses on 32-bit Linux system are 32 bits, and the 
// page size is still 4096 (12 bits).
// We split the remaining 20 bits evenly into top-level (page directory) index
// and second level (page table) index, using 10 bits for each.
#define PGDIR_SHIFT       22     // Leaves just top 10 bits of vaddr 
#define PTRS_PER_PGDIR  1024
#define PTRS_PER_PGTBL  1024

#endif

#define PGTBL_MASK        (PTRS_PER_PGTBL-1)
#define PGDIR_INDEX(x)   ((x) >> PGDIR_SHIFT)
#define PGTBL_INDEX(x)   (((x) >> PAGE_SHIFT) & PGTBL_MASK)


typedef unsigned long addr_t;

// These defines allow us to take advantage of the compiler's typechecking

// Page directory entry (top-level)
typedef struct { 
	uintptr_t pde; 
} pgdir_entry_t;

// Page table entry (2nd-level). 
typedef struct { 
	unsigned int frame; // if valid bit == 1, physical frame holding vpage
	off_t swap_off;       // offset in swap file of vpage, if any
} pgtbl_entry_t;    

extern void init_pagetable();
extern char *find_physpage(addr_t vaddr, char type);

extern void print_pagedirectory(void);

struct frame {
	char in_use;       // True if frame is allocated, False if frame is free
	pgtbl_entry_t *pte;// Pointer back to pagetable entry (pte) for page
	                   // stored in this frame
	int prev;   //lru linked list member which represents the which is prev of current page;
	int next;	//lru linked list member which represents the which is prev of current page;
	int ref;    // clock referenced which has value 1 and 0;

	int lru_prev;    // arc algo which represents T1(lru) linked list;
	int lru_next;	// arc algo which represents T1(lru) linked list;
	int lfu_prev;	// arc algo which represents T2(lfu) linked list;
	int lfu_next;	// arc algo which represents T2(lfu) linked list;
	
};

// frame referenced in arc algo when it move to B1 or B2;
struct frame_ghost {
	int lru_ghost_prev;   	// arc algo which represents B1(lru_ghost) linked list
	int lru_ghost_next;		// arc algo which represents B1(lru_ghost) linked list
	int lfu_ghost_prev;		// arc algo which represents B2(lfu_ghost) linked list
	int lfu_ghost_next;		// arc algo which represents B2(lfu_ghost) linked list
	
};

// representes 1 when we decide use arc algorithm;
int arc_use;

// arc algo, which represents case 4
int complete_miss;

// arc algo, which represents case 2
int ref_B1;

// arc algo, which represents case 3
int ref_B2;

// review page when met case 2, 3 or 4
int review_index;

// determine we delete first or delete after ref;
int evict_used;

/* The coremap holds information about physical memory.
 * The index into coremap is the physical page frame number stored
 * in the page table entry (pgtbl_entry_t).
 */
extern struct frame *coremap;

// The coremap_ghost holds frame which evicted into B1 or B2;
struct frame_ghost *coremap_ghost;


// Swap functions for use in other files
extern int swap_init(unsigned swapsize);
extern void swap_destroy(void);
extern int swap_pagein(unsigned frame, int swap_offset);
extern int swap_pageout(unsigned frame, int swap_offset);

extern void rand_init();
extern void lru_init();
extern void clock_init();
extern void fifo_init();
extern void arc_init();

// These may not need to do anything for some algorithms
extern void rand_ref(pgtbl_entry_t *);
extern void lru_ref(pgtbl_entry_t *);
extern void clock_ref(pgtbl_entry_t *);
extern void fifo_ref(pgtbl_entry_t *);
extern void arc_ref(pgtbl_entry_t *);

extern int rand_evict();
extern int lru_evict();
extern int clock_evict();
extern int fifo_evict();
extern int arc_evict();

#endif /* PAGETABLE_H */
