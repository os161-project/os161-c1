#ifndef __PT__
#define __PT__
#include <types.h>

typedef struct pT *page_table;

#include "swapfile.h"

page_table pageTInit(uint32_t n_pages);

// Add a new entry into page table, set V, set next and chain bit to zero.
void addEntry(page_table pt, uint32_t page_n, uint32_t index, uint32_t pid);

// Return the index (frame number) where page number is stored in, if page is not stored in memory, return -1
int getFrameAddress(page_table pt, uint32_t page_n, bool frame);

// Load new Page in the process address space, doing a swap-in from swapfile
paddr_t pageIn (page_table pt, uint32_t pid, vaddr_t vaddr, swap_table st);

uint32_t replace_page(page_table pt);

void all_proc_page_out(page_table pt);

paddr_t alloc_n_contiguos_pages(uint32_t npages, page_table pt);

paddr_t insert_page(page_table pt, vaddr_t vaddr, swap_table ST, int suggested_frame_n);

void remove_page(page_table pt, uint32_t frame_n);

void pages_fork(page_table pt, uint32_t start_src_frame, pid_t dst_pid);

void print_pt(page_table pt);

void print_FIFO(page_table pt);

#endif