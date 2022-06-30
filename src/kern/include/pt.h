#ifndef __PT__
#define __PT__
#include <types.h>

typedef struct pT *page_table;

page_table pageTInit(uint32_t n_pages);

//Add a new entry into page table, set V, set next and chain bit to zero.
void addEntry(page_table pt, uint32_t page_n, uint32_t index, uint32_t pid);

//Return the index where page number is stored in, if page is not stored in memory, return -1
int getFrameN(page_table pt, uint32_t page_n);

void setInvalid(page_table pt, uint32_t index);

//Use kfree function
void pageTFree(page_table pt);

uint32_t replace_page(page_table pt);

#endif