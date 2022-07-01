#ifndef __SWAPFILE__
#define __SWAPFILE__

#include <types.h>
#include <pt.h>

typedef struct swapTable *swap_table;

swap_table swapTableInit(char swap_file_name[]);

swap_table getSwapTable();

void swapout(swap_table st, uint32_t index, paddr_t paddr, page_table pt);

void swapin(swap_table st, uint32_t index, paddr_t paddr);

int getFirstFreeChunckIndex(swap_table st);


#endif