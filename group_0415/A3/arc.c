#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int P; //"target" size of T1

int delta;

int lru_size; // |T1|;
int lru_head; // MRU(T1);
int lru_tail; // LRU(T1);

int lfu_size; // |T2|;
int lfu_head; // MRU(T2);
int lfu_tail; // LRU(T2);

int lru_ghost_size; // |B1|;
int lru_ghost_head; // MRU(B1);
int lru_ghost_tail; // LRU(B1);

int lfu_ghost_size; // |B2|;
int lfu_ghost_head; // MRU(B2);
int lfu_ghost_tail; // LRU(B2);

int evict_frame; // frame which to be evicted;

/* REPLACE helper.
*/
void replace(){
	if (lru_size != 0 && (lru_size > P || (ref_B2 == 1 || lru_size == P))){
		// Evict LRU(T1)
		evict_frame = lru_tail;
		lru_tail = coremap[evict_frame].lru_prev;
		coremap[evict_frame].lru_prev = -1;
		coremap[lru_tail].lru_next= -1;

		if(lru_size == 1){
			lru_head = -1;
		}

		lru_size--;

		// move its reference to MRU(B1)
		if (lru_ghost_size == 0){
			lru_ghost_head = evict_frame;
			lru_ghost_tail = evict_frame;
		}else{
			coremap_ghost[evict_frame].lru_ghost_next = lru_ghost_head;
			coremap_ghost[lru_ghost_head].lru_ghost_prev = evict_frame;
			lru_ghost_head = evict_frame;
		}

		lru_ghost_size++;
	}else{
		// Evict LRU(T2)
		evict_frame = lfu_tail;
		lfu_tail = coremap[evict_frame].lfu_prev;
		coremap[evict_frame].lfu_prev = -1;
		coremap[lfu_tail].lfu_next = -1;


		if(lfu_size == 1){
			lfu_head = -1;
		}

		lfu_size--;

		// move its reference to MRU(B1)
		if (lfu_ghost_size == 0){
			lfu_ghost_head = evict_frame;
			lfu_ghost_tail = evict_frame;
		}else{
			coremap_ghost[evict_frame].lfu_ghost_next = lfu_ghost_head;
			coremap_ghost[lfu_ghost_head].lfu_ghost_prev = evict_frame;
			lfu_ghost_head = evict_frame;
		}

		lfu_ghost_size++;
	}
}

/* The page to evict is chosen using the ARC algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int arc_evict() {
	// If we allocate physical frame first, the page was first access;
	if (complete_miss == 1){
		// Case A: |T1| + |B1| = c pages
		if (lru_size + lru_ghost_size == memsize){
			if (lru_size < memsize){
				// delete reference in the LRU(B1) slot
				int tmp = lru_ghost_tail;
				lru_ghost_tail = coremap_ghost[lru_ghost_tail].lru_ghost_prev;
				coremap_ghost[tmp].lru_ghost_prev = -1;
				coremap_ghost[lru_ghost_tail].lru_ghost_next = -1;

				if(lru_ghost_size == 1){
					lru_ghost_head = -1;
				}

				lru_ghost_size--;

				//Apply REPLACE
				replace();
			}else{
				// delete page in the LRU(T1) slot (and evict the page)
				evict_frame = lru_tail;
				lru_tail = coremap[evict_frame].lru_prev;
				coremap[evict_frame].lru_prev = -1;
				coremap[lru_tail].lru_next = -1;

				if(lru_size == 1){
					lru_head = -1;
				}

				lru_size--;
			}
		}else{
			if (lru_size + lfu_size + lru_ghost_size + lfu_ghost_size >= memsize){
				if (lru_size + lfu_size + lru_ghost_size + lfu_ghost_size == (2 * memsize)){
					//Delete LRU(B2)
					int tmp = lfu_ghost_tail;
					lfu_ghost_tail = coremap_ghost[tmp].lfu_ghost_prev;
					coremap_ghost[tmp].lfu_ghost_prev = -1;
					coremap_ghost[lfu_ghost_tail].lfu_ghost_next = -1;

					if(lfu_ghost_size == 1){
						lfu_ghost_head = -1;
					}

					lfu_ghost_size--;
				}
				replace();
			}
		}
	}
	// If we allocate physical frame first, the page was miss and page in B1;
	else if (ref_B1 == 1){
		// Update P;
		delta =  (lru_ghost_size >= lfu_ghost_size)? 1 : (lfu_ghost_size / lru_ghost_size);

		if (P + delta < memsize){
			P = P + delta;
		}else{
			P = memsize;
		}

		// Apply REPLACE;
		replace();
	}
	// If we allocate physical frame first, the page was miss and page in B2;
	else if (ref_B2 == 1){
		// Update P;
		delta =  (lfu_ghost_size >= lru_ghost_size)? 1 : (lru_ghost_size / lfu_ghost_size);

		if (P - delta > 0){
			P = P - delta;
		}else{
			P = 0;
		}
		// Apply REPLACE;
		replace();
	}
	evict_used = 1;
	return evict_frame;
}

/* This function is called on each access to a page to update any information
 * needed by the ARC algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void arc_ref(pgtbl_entry_t *p) {
	// Remove 12 bits of status and get the frame number;
	int index = (p->frame)>>PAGE_SHIFT;

	// Case 1: access is in T1 or T2;
	// Case 1.1: Hit in T1;
	if (coremap[index].lru_next != -1 || coremap[index].lru_prev != -1){
		// Remove from T1;
		// If frame was head(MRU(T1));
		if (lru_head == index){
			// Set head to lru_head's next;
			lru_head = coremap[index].lru_next;
			// Update the linked list relations;
			coremap[lru_head].lru_prev = -1;
			coremap[index].lru_next = -1;

			if(lru_size == 1){
				lru_tail = -1;
			}
		}
		// If frame was tail(LRU(T1));
		else if (lru_tail == index){
			// Set tail to lru_tail' prev;
			lru_tail = coremap[index].lru_prev;
			// Update the linked list relations;
			coremap[lru_tail].lru_next = -1;
			coremap[index].lru_prev = -1;
		}
		// If frame was in middle of T1;
		else{
			int tmp = coremap[index].lru_prev;
			int tmp2 = coremap[index].lru_next;
			coremap[tmp].lru_next = tmp2;
			coremap[tmp2].lru_prev = tmp;

			coremap[index].lru_prev = -1;
			coremap[index].lru_next = -1;
		}
		// Shrink T1 size by 1;
		lru_size--;

		// Move access page to MRU(T2) which is the head of lfu;
		if (lfu_size == 0){
			lfu_head = index;
			lfu_tail = index;
		}else{
			coremap[index].lfu_next = lfu_head;
			coremap[lfu_head].lfu_prev = index;
			lfu_head = index;
		}

		// Increase T2 size by 1;
		lfu_size++;
	}
	// Case 1.2: Hit in T2;
	else if (coremap[index].lfu_prev != -1 || coremap[index].lfu_next != -1){
		// Access the page to MRU(T2) which is the head of lfu;
		// If frame was head of T2, do nothing;
		if (lfu_head == index){
			return;
		}
		// If frame was tail of T2;
		else if (lfu_tail == index){
			lfu_tail = coremap[index].lfu_prev;
			coremap[lfu_tail].lfu_next = -1;
			coremap[index].lfu_prev = -1;
		}
		// If frame was middle of T2;
		else{
			int tmp = coremap[index].lfu_prev;
			int tmp2 = coremap[index].lfu_next;
			coremap[tmp].lfu_next = tmp2;
			coremap[tmp2].lfu_prev = tmp;

			coremap[index].lfu_prev = -1;
			coremap[index].lfu_next = -1;
		}

		// Move access page to MRU(T2) which is the head of lfu;
		if (lfu_size == 0){
			lfu_head = index;
			lfu_tail = index;
		}else{
			coremap[index].lfu_next = lfu_head;
			coremap[lfu_head].lfu_prev = index;
			lfu_head = index;
		}
	}

	// Case 2: we dont need to allocate space first, miss but hit in B1;
	if (ref_B1 == 1){
		// evict_used == 0 means we have space for this page
		if(evict_used == 0){
			// Update P;
			delta =  (lru_ghost_size >= lfu_ghost_size)? 1 : (lfu_ghost_size / lru_ghost_size);
			if (P + delta < memsize){
				P = P + delta;
			}else{
				P = memsize;
			}
			// Apply REPLACE;
			replace();
		}

		// Move reference from B1 into MRU(T2) (and fetch the page)
		if (review_index == lru_ghost_head){
			lru_ghost_head = coremap_ghost[review_index].lru_ghost_next;
			coremap_ghost[review_index].lru_ghost_next = -1;
			coremap_ghost[lru_ghost_head].lru_ghost_prev = -1;


			if(lru_ghost_size == 1){
				lru_ghost_tail = -1;
			}
		}else if (review_index == lru_ghost_tail){
			lru_ghost_tail = coremap_ghost[review_index].lru_ghost_prev;
			coremap_ghost[review_index].lru_ghost_prev = -1;
			coremap_ghost[lru_ghost_tail].lru_ghost_next = -1;
		}else{
			int tmp = coremap_ghost[review_index].lru_ghost_prev;
			int tmp2 = coremap_ghost[review_index].lru_ghost_next;
			coremap_ghost[tmp].lru_ghost_next = tmp2;
			coremap_ghost[tmp2].lru_ghost_prev = tmp;

			coremap_ghost[review_index].lru_ghost_next = -1;
			coremap_ghost[review_index].lru_ghost_prev = -1;
		}

		lru_ghost_size--;

		coremap[index].lfu_next = lfu_head;
		coremap[lfu_head].lfu_prev = index;
		lfu_head = index;

		lfu_size++;

	} 
	// Case 3: we dont need to allocate space first, miss but hit in B2;
	else if (ref_B2 == 1){
		//evict_used == 0 means we have space for this page
		if(evict_used == 0){
			// Update P;
			delta =  (lfu_ghost_size >= lru_ghost_size)? 1 : (lru_ghost_size / lfu_ghost_size);
			if (P - delta > 0){
				P = P - delta;
			}else{
				P = 0;
			}
			// Apply REPLACE;
			replace();
		}

		// Move from B2 reference into MRU(T2) (and fetch the page)
		if (review_index == lfu_ghost_head){
			lfu_ghost_head = coremap_ghost[review_index].lfu_ghost_next;
			coremap_ghost[review_index].lfu_ghost_next = -1;
			coremap_ghost[lfu_ghost_head].lfu_ghost_prev = -1;


			if(lfu_ghost_size == 1){
				lfu_ghost_tail = -1;
			}
		}else if (review_index == lfu_ghost_tail){
			lfu_ghost_tail = coremap_ghost[review_index].lfu_ghost_prev;
			coremap_ghost[review_index].lfu_ghost_prev = -1;
			coremap_ghost[lfu_ghost_tail].lfu_ghost_next = -1;
		}else{
			int tmp = coremap_ghost[review_index].lfu_ghost_prev;
			int tmp2 = coremap_ghost[review_index].lfu_ghost_next;
			coremap_ghost[tmp].lfu_ghost_next = tmp2;
			coremap_ghost[tmp2].lfu_ghost_prev = tmp;

			coremap_ghost[review_index].lfu_ghost_next = -1;
			coremap_ghost[review_index].lfu_ghost_prev = -1;
		}

		lfu_ghost_size--;

		coremap[index].lfu_next = lfu_head;
		coremap[lfu_head].lfu_prev = index;
		lfu_head = index;

		lfu_size++;
	}
	// Case 4: we have space for access page, the page was first access;
	else if (complete_miss == 1){
		//evict_used == 0 means we have space for this page
		if(evict_used == 0){
			if (lru_size + lru_ghost_size == memsize){
				if (lru_size < memsize){
					// delete reference in the LRU(B1) slot
					int tmp = lru_ghost_tail;
					lru_ghost_tail = coremap_ghost[lru_ghost_tail].lru_ghost_prev;
					coremap_ghost[tmp].lru_ghost_prev = -1;
					coremap_ghost[lru_ghost_tail].lru_ghost_next = -1;

					lru_ghost_size--;

					if(lru_ghost_size == 1){
						lru_ghost_head = -1;
					}

					//Apply REPLACE
					replace();
				}else{
					// delete page in the LRU(T1) slot (and evict the page)
					evict_frame = lru_tail;
					lru_tail = coremap[evict_frame].lru_prev;
					coremap[evict_frame].lru_prev = -1;
					coremap[lru_tail].lru_next = -1;

					if(lru_size == 1){
						lru_head = -1;
					}

					lru_size--;
				}
			}else{
				if (lru_size + lfu_size + lru_ghost_size + lfu_ghost_size > memsize){
					if (lru_size + lfu_size + lru_ghost_size + lfu_ghost_size == (2 * memsize)){
						//Delete LRU(B2)
						int tmp = lfu_ghost_tail;
						lfu_ghost_tail = coremap_ghost[tmp].lfu_ghost_prev;
						coremap_ghost[tmp].lfu_ghost_prev = -1;
						coremap_ghost[lfu_ghost_tail].lfu_ghost_next = -1;

						if(lfu_ghost_size == 1){
							lfu_ghost_head = -1;
						}

						lfu_ghost_size--;
					}
					//Apply REPLACE
					replace();
				}
			}
		}
		// Finally, fetch the page into memory and put it in the MRU(T1);
		if (lru_head == -1){
			lru_head = index;
			lru_tail = index;
		}else{
			coremap[index].lru_next = lru_head;
			coremap[lru_head].lru_prev = index;
			lru_head = index;
		}

		lru_size++;
	}
	evict_used = 0;
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void arc_init() {
	// When we used arc algo, we set this as 1;
	arc_use = 1;
	complete_miss = 0;
	ref_B1 = 0;
	ref_B2 = 0;	
	evict_used = 0;
	// Initialize the P which equal to T1 "target" size, starts at 0;
	P = 0;

	// Initialize T1, T2, B1 and B2 sizes;
	lru_size = 0;
	lfu_size = 0;
	lru_ghost_size = 0;
	lfu_ghost_size = 0;

	// Initialize MRU and LRU of T1, T2, B1 and B2;
	lru_head = -1;
	lru_tail = -1;

	lfu_head = -1;
	lfu_tail = -1;

	lru_ghost_head = -1;
	lru_ghost_tail = -1;
	
	lfu_ghost_head = -1;
	lfu_ghost_tail = -1;

	coremap_ghost = calloc(memsize, sizeof(struct frame));
	for (int i = 0; i < memsize; i++){
		coremap_ghost[i].lru_ghost_prev = -1;
		coremap_ghost[i].lru_ghost_next = -1;

		coremap_ghost[i].lfu_ghost_prev = -1;
		coremap_ghost[i].lfu_ghost_next = -1;
	}

	for (int j = 0; j < memsize; j++){
		coremap[j].lru_prev = -1;
		coremap[j].lru_next = -1;

		coremap[j].lfu_prev = -1;
		coremap[j].lfu_next = -1;
	}
}

