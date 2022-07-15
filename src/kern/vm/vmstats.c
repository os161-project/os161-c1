#include <vmstats.h>
#include <spinlock.h>


#define SASSERT(x) ((x) ? (void)0 : statassert(#x, __FILE__, __LINE__,  __func__))
#define VERBOSE 0

#if VERBOSE
static void
statassert(const char *expr, const char *file, int line, const char *func)
{
	kprintf("Warning (inequality): %s, at %s:%d (%s)\n",
	      expr, file, line, func);
}
#endif

struct statistics {
    uint32_t    tlb_faults_total,
                tlb_faults[2],      // Free or Replace
                tlb_invalidations,  
                tlb_reloads, 
                page_faults[4],     // Zeroed, Disk, Elf, Swapfile
                swap_writes,
                swap_chunks[2];     // Zero-filled, Blank Chunks
    struct spinlock lock;
} stat;

void
stat_bootstrap(void) {
    spinlock_init(&stat.lock);
    stat.tlb_faults_total = 0;
    stat.tlb_faults[0] = 0;
    stat.tlb_faults[1] = 0;
    stat.tlb_invalidations = 0;
    stat.tlb_reloads = 0;
    stat.page_faults[0] = 0; 
    stat.page_faults[1] = 0;
    stat.page_faults[2] = 0;
    stat.page_faults[3] = 0;
    stat.swap_writes = 0;
}

void
add_TLB_fault(void) {
    spinlock_acquire(&stat.lock);
    stat.tlb_faults_total++;
    spinlock_release(&stat.lock);  
}

void
add_TLB_fault_type(int free_entry) {
    spinlock_acquire(&stat.lock);
    stat.tlb_faults[free_entry]++;
    spinlock_release(&stat.lock);
}

void
add_TLB_invalidation(void) {
    spinlock_acquire(&stat.lock);
    stat.tlb_invalidations++;
    spinlock_release(&stat.lock);
}

void
add_TLB_reload(void) {
    spinlock_acquire(&stat.lock);
    stat.tlb_reloads++;
    spinlock_release(&stat.lock);
}

void
add_VM_pageFault(int type) {
    spinlock_acquire(&stat.lock);
    stat.page_faults[type]++;
    spinlock_release(&stat.lock);
}

void
add_SWAP_write(void) {
    spinlock_acquire(&stat.lock);
    stat.swap_writes++;
    spinlock_release(&stat.lock);
}

void 
add_SWAP_chunk(int type) {
    spinlock_acquire(&stat.lock);
    stat.swap_chunks[type]++;
    spinlock_release(&stat.lock);
}


void
print_stats(void) {
    /* Check possible inequalities (a.k.a. buggy behaviors) */
    spinlock_acquire(&stat.lock);
#if VERBOSE
    SASSERT(stat.tlb_faults_total == stat.tlb_faults[TLB_FREE] + stat.tlb_faults[TLB_REPLACE]);
    SASSERT(stat.tlb_faults_total == stat.tlb_reloads + stat.page_faults[VM_DISK] + stat.page_faults[VM_ZEROED]);
    SASSERT(stat.page_faults[VM_DISK] == stat.page_faults[VM_ELF] + stat.page_faults[VM_SWAP]);
#endif
    kprintf("[vm] Collected statistics:\n");
    kprintf("[vm] TLB Faults - Total: %5d, Free: %5d, Replaced: %5d\n", 
                                stat.tlb_faults_total, stat.tlb_faults[TLB_FREE], stat.tlb_faults[TLB_REPLACE]);
    kprintf("[vm] TLB Invalidations - Total: %5d\n", stat.tlb_invalidations);
    kprintf("[vm] TLB Reloads - Total: %5d\n", stat.tlb_reloads);
    uint32_t total_page_faults = stat.page_faults[VM_ZEROED] + stat.page_faults[VM_DISK] +
                                stat.page_faults[VM_ELF] + stat.page_faults[VM_SWAP];
    kprintf("[vm] Page Faults - Total: %5d, Zeroed: %5d, Disk: %5d, ELF: %5d, Swapfile: %5d\n", 
                                total_page_faults, stat.page_faults[VM_ZEROED], 
                                stat.page_faults[VM_DISK], stat.page_faults[VM_ELF], stat.page_faults[VM_SWAP]); 
    kprintf("[vm] Swapfile Writes - Total: %5d\n", stat.swap_writes);
    kprintf("[vm] Swapfile Chunks - Zero-filled: %5d, Blank: %5d\n", stat.swap_chunks[SWAP_0_FILLED], stat.swap_chunks[SWAP_BLANK]);
    spinlock_release(&stat.lock);
}   

