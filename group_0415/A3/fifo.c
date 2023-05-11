#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int current_head;

/* The page to evict is chosen using the FIFO algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
	// Store current page frame number which will be evicted;
	int tmp = current_head;
	// Since we only contain 'memsize' pages in list;
	// When we evict one frame, we move to the head to the next index of frame
	// which was added next to the current evicted frame;
	current_head = (current_head + 1) % memsize;
	return tmp;
}

/* This function is called on each access to a page to update any information
 * needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {
	return;
}

/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {
	// The first page adding is the first items which at index 0;
	current_head = 0;
}
