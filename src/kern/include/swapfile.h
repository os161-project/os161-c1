#ifndef __SWAPFILE__
#define __SWAPFILE__

#include <types.h>
#include <vnode.h>

typedef struct swapTable *swap_table;

#include "pt.h"

swap_table swapTableInit(char swap_file_name[]);

swap_table getSwapTable();

void swapout(swap_table st, uint32_t index, paddr_t paddr, page_table pt);

void swapin(swap_table st, uint32_t index, paddr_t paddr);

int getFirstFreeChunckIndex(swap_table st);

void elf_to_swap(swap_table st, struct vnode *v, off_t offset, uint32_t init_page_n, size_t memsize, uint32_t PID);

#endif