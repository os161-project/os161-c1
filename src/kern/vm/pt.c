#include "pt.h"
#include "swapfile.h"
#include <types.h>
#include <current.h>
#include <proc.h>
#include <vm.h>
#include <spl.h>
#include <stdlib.h>

// V = validity bit
// C = chain bit (if next field has a value)
//<----------------20------------>|<----6-----><----6---->|
//_________________________________________________________
//|       Virtual Page Number     |                   |C|V|  hi
//|_______________________________|_______________________|
//|       Next                    |           |    PID    |  low
//|_______________________________|_______________________|

#define IS_VALID(x) ((x) & 0x00000001)
#define SET_VALID(x, value) (((x) &~ 0x00000001) | value)
#define SET_PN(entry, pn) ((entry & 0x00000FFF) | (pn << 12))
#define GET_PN(entry) ((entry &~ 0x00000FFF) >> 12)
#define SET_NEXT(entry, next) ((entry & 0x00000FFF) | (next << 12))
#define GET_NEXT(entry) ((entry &~ 0x00000FFF) >> 12)
#define SET_PID(entry, pid) ((entry &~ 0x0000003F) | pid)
#define GET_PID(entry) (entry & 0x0000003F)
#define HAS_CHAIN(x) ((x) & 0x00000002)
#define SET_CHAIN(x, value) (((x) &~ 0x00000002) | (value << 1))

struct PTE{
    uint32_t hi, low;
};

struct pT{
    struct PTE *entries;
    uint32_t size;
};

page_table pageTInit(uint32_t n_pages){
    uint32_t i;
    page_table tmp = kmalloc(sizeof(*tmp));
    tmp->entries = kmalloc(n_pages * sizeof(*(tmp->entries)));
    tmp->size = n_pages;
    for(i = 0; i < n_pages; i++){
        tmp->entries[i].hi = SET_PN(tmp->entries[i].hi, 0) | SET_VALID(tmp->entries[i].hi, 0) | SET_CHAIN(tmp->entries[i].hi, 0);
        tmp->entries[i].low = SET_NEXT(tmp->entries[i].low, 0) | SET_PID(tmp->entries[i].low, 0);
    }
    return tmp;
}

//Add a new entry into page table, set V, set next and chain bit to zero.
void addEntry(page_table pt, uint32_t page_n, uint32_t index, uint32_t pid){
    pt->entries[index].hi = SET_PN(pt->entries[index].hi, page_n) | SET_CHAIN(pt->entries[index].hi, 0) | SET_VALID(pt->entries[index].hi, 1);
    pt->entries[index].low = SET_PID(pt->entries[index].low, pid) | SET_NEXT(pt->entries[index].low, 0);
    return;
}

//Return the index where page number is stored in, if page is not stored in memory, return -1
int getFrameN(page_table pt, uint32_t page_n){
    //KASSERT(GET_PID(pt->entries[curproc->start_pt_i].low) == curproc->p_pid);
    for(int i = curproc->start_pt_i; i != -1 && HAS_CHAIN(pt->entries[i].hi); i = GET_NEXT(pt->entries[i].low)){
        if(GET_PN(pt->entries[i].hi) == page_n){
            return i;
        }
    }
    return -1;
}

// Return the page number for a given page table entry (corresponding to the given index)
uint32_t getPageN(page_table pt, uint32_t index) {
    KASSERT(pt->entries != NULL);
    return GET_PN(pt->entries[index].hi);
}

// Return the PID stored in a page table entry, corresponding to the given index
uint32_t getPID(page_table pt, uint32_t index) {
    KASSERT(pt->entries != NULL);
    return GET_PID(pt->entries[index].low);
}

void setInvalid(page_table pt, uint32_t index){
    pt->entries[index].hi = SET_VALID(pt->entries[index].hi, 0);
    return;
}

//Use kfree function
void pageTFree(page_table pt){
    kfree(pt->entries);
    kfree(pt);
    return;
}

uint32_t replace_page(page_table pt){
    int spl = splhigh();
    uint32_t page_index = random() % pt->size;
    splx(spl);
    return page_index;
}


void pageIn(page_table pt, uint32_t pid, paddr_t paddr) {
    int spl;
    uint32_t index = paddr / PAGE_FRAME;
    // Code or Data?
    vaddr_t vaddr = paddr + MIPS_KUSEG;
    // TODO: Check if the virtual address is from the code or data segment
    // TODO: Before swapping-in a new frame from the swap-file, we may need to make
    // room for a new one, if the page table is already full.
    // In this case, we need a page replacement algorithm, for selecting a victim to evict.
    // The evicted page will be swapped-out from the IPT and loaded into the Swapfile
    // Swap-in the frame from the swapfile, loading it into kernel buffer (?)
    // TODO: Load the frame directly to the address space of the process
    swapin(swap_table, index, paddr);
    spl = splhigh();
    // TODO: Set vpage number 
    pt->entries[index].low = SET_PID(pt->entries[index], pid);
    pt->entries[index].hi = SET_VALID(pt->entries, 1) | SET_CHAIN(pt->entries, 0);
    splx(spl);
}