#ifndef __STAT__
#define __STAT__

#include <types.h>
#include <lib.h>

#define TLB_FREE    1
#define TLB_REPLACE 0

#define VM_ZEROED   0
#define VM_DISK     1
#define VM_ELF      2
#define VM_SWAP     3

#define SWAP_0_FILLED   0
#define SWAP_BLANK      1   


void stat_bootstrap(void);

/* Increase the number of times TLB misses have occured */
void add_TLB_fault(void);
/* 
    Increase the number of times TLB misses have occured, after checking the TLB status
    free_entry == 1 means free space in the TLB (free entry)
    free_entry == 0 means no space in the TLB, so replacement is required 
*/
void add_TLB_fault_type(int free_entry);

/* Number of times the *entire* TLB was invalidated (not the total entries invalidated) */
void add_TLB_invalidation(void);

/* Number of TLB misses for pages already in memory */
void add_TLB_reload(void);

/* 
    type == VM_ZEROED, page fault that requires a new page to be zero-filled
    type == VM_DISK, page fault that requires a page to be loaded from disk
    type == VM_ELF, page fault that requires getting a page from the ELF file     
    type == VM_SWAP, number of page faults that require getting a page from the swap file.   
*/
void add_VM_pageFault(int type);

/* Number of page faults that require writing a page to the swap file */
void add_SWAP_write(void);


/* Number of chunks zero-filled or blank (all zeros) on the swap file */
void add_SWAP_chunk(int type);

/* Print out statistics and eventually some warnings, when shooting down the VM system */
void print_stats(void);

#endif