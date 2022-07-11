#ifndef __SWAPFILE__
#define __SWAPFILE__

#include <types.h>
#include <vnode.h>

typedef struct swapTable *swap_table;

#include "pt.h"

swap_table swapTableInit(char swap_file_name[]);

void swapout(swap_table st, uint32_t index, paddr_t paddr, uint32_t page_number, uint32_t pid, bool invalidate);

void swapin(swap_table st, uint32_t index, paddr_t paddr);

int getFirstFreeChunckIndex(swap_table st);

void elf_to_swap(swap_table st, struct vnode *v, off_t offset, uint32_t init_page_n, size_t memsize, pid_t PID);

int getSwapChunk(swap_table st, vaddr_t faultaddress, pid_t pid);

void all_proc_chunk_out(swap_table st);

void chunks_fork(swap_table st, pid_t src_pid, pid_t dst_pid);

void print_chunks(swap_table st);

void checkDuplicatedEntries(swap_table st);

void delete_free_chunk(swap_table st,uint32_t chunk_to_delete);

void insert_into_free_chunk_list(swap_table st, uint32_t chunk_to_add);

#endif