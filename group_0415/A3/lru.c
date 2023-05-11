#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int head;
int tail;

/* The page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	int evicted = tail;
	if (head == tail){
		head = -1;
		tail = -1;
	}else{
		int prev = coremap[tail].prev;
		coremap[prev].next = -1;
		tail = prev;
	}
	coremap[evicted].next = -1;
	coremap[evicted].prev = -1;
	return evicted;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
	// Remove 12 bits of status and get the frame number;
	int index = (p->frame)>>PAGE_SHIFT;
	// If it is the first frame added into the linked list;
	if (head == -1 && tail == -1){
		// head and tail reference to the first item;
		head = index;
		tail = index;
	}else if (head != index){
		// if frame was at head postion, we do nothing because it has 
		// the highest piority; if frame was not, then we have two
		// cases;

		// Check whether the frame was already in list or not;
		// If frame was in list;
		if (!(coremap[index].prev == -1 && coremap[index].next == -1)){
			int prev;
			int next;
			// If frame was tail;
			if (tail == index){
				// Set frame's prev
				prev = coremap[index].prev;
				coremap[prev].next = -1;
				coremap[index].prev = -1;
				tail = prev;
			}else{
				// If frame was not tail, set the frame's prev and next;
				prev = coremap[index].prev;
				next = coremap[index].next;
				coremap[index].prev = -1;
				coremap[prev].next = next;
				coremap[next].prev = prev;
			}	
		}
		coremap[index].next = head;
		coremap[head].prev = index;
		head = index;
		
	}
	return;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
	// Initialize prev and next for each frame index;
	for(int i=0; i < memsize; i++){
		coremap[i].prev = -1;
		coremap[i].next = -1;
	}
	// Initialize linked list head and tail;
	head = -1;
	tail = -1;
}
