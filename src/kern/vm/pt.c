#include "pt.h"
#include <types.h>
#include <current.h>
#include <proc.h>
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
