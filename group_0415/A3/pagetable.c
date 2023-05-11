#include <assert.h>
#include <string.h> 
#include "sim.h"
#include "pagetable.h"

// The top-level page table (also known as the 'page directory')
pgdir_entry_t pgdir[PTRS_PER_PGDIR]; 

// Counters for various events.
// Your code must increment these when the related events occur.
int hit_count = 0;
int miss_count = 0;
int ref_count = 0;
int evict_clean_count = 0;
int evict_dirty_count = 0;

/*
 * Allocates a frame to be used for the virtual page represented by p.
 * If all frames are in use, calls the replacement algorithm's evict_fcn to
 * select a victim frame. Writes victim to swap if needed, and updates 
 * pagetable entry for victim to indicate that virtual page is no longer in
 * (simulated) physical memory.
 *
 * The counters for evictions should be updated appropriately in this function.
 */
int allocate_frame(pgtbl_entry_t *p) {
	int i;
	int frame = -1;
	for(i = 0; i < memsize; i++) {
		if(!coremap[i].in_use) {
			frame = i;
			break;
		}
	}
	if(frame == -1) { // Didn't find a free page.
		// Call replacement algorithm's evict function to select victim
		frame = evict_fcn();
		// All frames were in use, so victim frame must hold some page
		// Write victim page to swap, if needed, and update pagetable
		// IMPLEMENTATION NEEDED

		// Check the page is dirty or not;
		if ((coremap[frame].pte->frame) & PG_DIRTY){
			// If page was modified(dirty), update global count first;
			evict_dirty_count++;

			// Check whether the page can swap or not;
			int swap_offset = swap_pageout(frame, coremap[frame].pte->swap_off);
			// Data was written on failure;
			if (swap_offset == INVALID_SWAP){
				// return error message;
				perror("swap_pageout\n");
				exit(1);
			}

			// Update the offset in coremap[frame].pte if swap on success;
			coremap[frame].pte->swap_off = swap_offset;

			// Update the swap flag(bit) of coremap[frame].pte
			coremap[frame].pte->frame = (coremap[frame].pte->frame) | PG_ONSWAP;
			coremap[frame].pte->frame = (coremap[frame].pte->frame) & ~PG_DIRTY;
			
		}else{
			// If page was clean, update global count first;
			evict_clean_count++;
		}

		// Update the dirty flag(bit) of coremap[frame].pte

		// Update the valid flag(bit) of coremap[frame].pte
		coremap[frame].pte->frame = (coremap[frame].pte->frame) & ~PG_VALID;
	}

	// Record information for virtual page that will now be stored in frame
	coremap[frame].pte = p;
	coremap[frame].in_use = 1;

	return frame;
}

/*
 * Initializes the top-level pagetable.
 * This function is called once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is 
 * being simulated, so there is just one top-level page table (page directory).
 * To keep things simple, we use a global array of 'page directory entries'.
 *
 * In a real OS, each process would have its own page directory, which would
 * need to be allocated and initialized as part of process creation.
 */
void init_pagetable() {
	int i;
	// Set all entries in top-level pagetable to 0, which ensures valid
	// bits are all 0 initially.
	for (i=0; i < PTRS_PER_PGDIR; i++) {
		pgdir[i].pde = 0;
	}
}

// For simulation, we get second-level pagetables from ordinary memory
pgdir_entry_t init_second_level() {
	int i;
	pgdir_entry_t new_entry;
	pgtbl_entry_t *pgtbl;

	// Allocating aligned memory ensures the low bits in the pointer must
	// be zero, so we can use them to store our status bits, like PG_VALID
	if (posix_memalign((void **)&pgtbl, PAGE_SIZE, 
			   PTRS_PER_PGTBL*sizeof(pgtbl_entry_t)) != 0) {
		perror("Failed to allocate aligned memory for page table");
		exit(1);
	}

	// Initialize all entries in second-level pagetable
	for (i=0; i < PTRS_PER_PGTBL; i++) {
		pgtbl[i].frame = 0; // sets all bits, including valid, to zero
		pgtbl[i].swap_off = INVALID_SWAP;
	}

	// Mark the new page directory entry as valid
	new_entry.pde = (uintptr_t)pgtbl | PG_VALID;

	return new_entry;
}

/* 
 * Initializes the content of a (simulated) physical memory frame when it 
 * is first allocated for some virtual address.  Just like in a real OS,
 * we fill the frame with zero's to prevent leaking information across
 * pages. 
 * 
 * In our simulation, we also store the the virtual address itself in the 
 * page frame to help with error checking.
 *
 */
void init_frame(int frame, addr_t vaddr) {
	// Calculate pointer to start of frame in (simulated) physical memory
	char *mem_ptr = &physmem[frame*SIMPAGESIZE];
	// Calculate pointer to location in page where we keep the vaddr
        addr_t *vaddr_ptr = (addr_t *)(mem_ptr + sizeof(int));
	
	memset(mem_ptr, 0, SIMPAGESIZE); // zero-fill the frame
	*vaddr_ptr = vaddr;              // record the vaddr for error checking

	return;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the entry is invalid and not on swap, then this is the first reference 
 * to the page and a (simulated) physical frame should be allocated and 
 * initialized (using init_frame).  
 *
 * If the entry is invalid and on swap, then a (simulated) physical frame
 * should be allocated and filled by reading the page data from swap.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
char *find_physpage(addr_t vaddr, char type) {
	pgtbl_entry_t *p=NULL; // pointer to the full page table entry for vaddr
	unsigned idx = PGDIR_INDEX(vaddr); // get index into page directory

	// IMPLEMENTATION NEEDED
	// Use top-level page directory to get pointer to 2nd-level page table
	// To keep compiler happy - remove when you have a real use.
	// Now, compiler is sad.

	// First, we check whether the directory entry is valid or not;
	// Get the current page directory entry;
	uintptr_t current_pde = pgdir[idx].pde;

	if ((current_pde & PG_VALID) == 0){
		// This is invalid page directory entry;
		// Initial a valid 2nd-level page table for this directory entry;
		pgdir[idx] = init_second_level();
	}

	// Finding the 2nd-level page table by PAGE_MASK from page directory;
	pgtbl_entry_t *pgt = (pgtbl_entry_t *)(pgdir[idx].pde & PAGE_MASK); // second frame

	// Use vaddr to get index into 2nd-level page table and initialize 'p'

	// Get index into 2nd-level page table which is second 10 bits from left to right;
	unsigned pt_idx = PGTBL_INDEX(vaddr);

	// Set the page table entry to p;
	p = &(pgt[pt_idx]);
	
	// Check if p is valid or not, on swap or not, and handle appropriately
	
	// First case, if the entry p is invalid;
	if ((p->frame & PG_VALID) == 0){
		// If we used arc;
		if(arc_use == 1){
			int ghost_index = (p->frame)>>PAGE_SHIFT;
			// in B1 or B2 or completed miss
			if (coremap_ghost[ghost_index].lru_ghost_prev != -1 || coremap_ghost[ghost_index].lru_ghost_next != -1){
				ref_B1 = 1;
			}
			else if (coremap_ghost[ghost_index].lfu_ghost_prev != -1 || coremap_ghost[ghost_index].lfu_ghost_next != -1){
				ref_B2 = 1;
			}
			else{
				complete_miss = 1;
			}
			review_index = ghost_index;
		}
		// Build physical frame, allocate physical memory for frame;
		int frame = allocate_frame(p);
		// If it is not on swap;
		if ((p->frame & PG_ONSWAP) == 0){ 
			// Initialized the frame;
			init_frame(frame, vaddr);
		}else{
			// Read data into physical memory frame from swap_offset;
            int swap_in_page = swap_pagein(frame, p->swap_off);
            if (swap_in_page == INVALID_SWAP) {
                perror("swap_pagein\n");
                exit(1);
            }
			p->frame |= PG_ONSWAP;
		}
		// Set 12 bits to represents status;
		p->frame = frame << PAGE_SHIFT;

		if ((p->frame & PG_ONSWAP)){ 
			// Set the p is dirty;
            p->frame = p->frame | PG_DIRTY;
			
		}else{
			// Set the p is not dirty;
			p->frame = p->frame & (~PG_DIRTY);
		}
		// Update the global counters. If entry is invalid, it means memory miss;
		miss_count++;
	}else{
		// Update the global counters. If entry is valid, it means memory hit regardless of whether or not the page is or isn’t on swap;
		hit_count++;
	}


	// Make sure that p is marked valid and referenced. Also mark it
	// dirty if the access type indicates that the page will be written to.

	// Set p is valid;
	p->frame = p->frame | PG_VALID;
	// Set p is referenced
    p->frame = p->frame | PG_REF;
	
	// Check whether the page will be written to;
    if (type == 'S' || type == 'M') {
		// If so, set the page is dirty;
        p->frame = p->frame | PG_DIRTY;
    }
	// Update the global variable;
	ref_count++;

	// Call replacement algorithm's ref_fcn for this page
	ref_fcn(p);

	complete_miss = 0;
	ref_B1 = 0;
	ref_B2 = 0;

	// Return pointer into (simulated) physical memory at start of frame
	return  &physmem[(p->frame >> PAGE_SHIFT)*SIMPAGESIZE];
}

void print_pagetbl(pgtbl_entry_t *pgtbl) {
	int i;
	int first_invalid, last_invalid;
	first_invalid = last_invalid = -1;

	for (i=0; i < PTRS_PER_PGTBL; i++) {
		if (!(pgtbl[i].frame & PG_VALID) && 
		    !(pgtbl[i].frame & PG_ONSWAP)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("\t[%d] - [%d]: INVALID\n",
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			printf("\t[%d]: ",i);
			if (pgtbl[i].frame & PG_VALID) {
				printf("VALID, ");
				if (pgtbl[i].frame & PG_DIRTY) {
					printf("DIRTY, ");
				}
				printf("in frame %d\n",pgtbl[i].frame >> PAGE_SHIFT);
			} else {
				assert(pgtbl[i].frame & PG_ONSWAP);
				printf("ONSWAP, at offset %lu\n",pgtbl[i].swap_off);
			}			
		}
	}
	if (first_invalid != -1) {
		printf("\t[%d] - [%d]: INVALID\n", first_invalid, last_invalid);
		first_invalid = last_invalid = -1;
	}
}

void print_pagedirectory() {
	int i; // index into pgdir
	int first_invalid,last_invalid;
	first_invalid = last_invalid = -1;

	pgtbl_entry_t *pgtbl;

	for (i=0; i < PTRS_PER_PGDIR; i++) {
		if (!(pgdir[i].pde & PG_VALID)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("[%d]: INVALID\n  to\n[%d]: INVALID\n", 
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			pgtbl = (pgtbl_entry_t *)(pgdir[i].pde & PAGE_MASK);
			printf("[%d]: %p\n",i, pgtbl);
			print_pagetbl(pgtbl);
		}
	}
}
