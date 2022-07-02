#ifndef __PT__
#define __PT__
#include <types.h>

typedef struct pT *page_table;

#include "swapfile.h"

page_table pageTInit(uint32_t n_pages);

// Add a new entry into page table, set V, set next and chain bit to zero.
void addEntry(page_table pt, uint32_t page_n, uint32_t index, uint32_t pid);

// Return the index (frame number) where page number is stored in, if page is not stored in memory, return -1
int getFrameN(page_table pt, uint32_t page_n);

// Return the page number for a given page table entry (corresponding to the given index)
uint32_t getPageN(page_table pt, uint32_t index);

// Return the PID stored in a page table entry, corresponding to the given index
uint32_t getPID(page_table pt, uint32_t index);

void setInvalid(page_table pt, uint32_t index);

// Load new Page in the process address space, doing a swap-in from swapfile
void pageIn (page_table pt, uint32_t pid, paddr_t paddr, swap_table st);

//Use kfree function
void pageTFree(page_table pt);

uint32_t replace_page(page_table pt);

#endif