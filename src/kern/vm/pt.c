#include "pt.h"
#include <types.h>

// V = validity bit
// C = chain bit (if next field has a value)
//<----------------20------------>|<----6-----><----6---->|
//_________________________________________________________
//|       Virtual Page Number     |                   |C|V|  hi
//|_______________________________|_______________________|
//|       Next                    |           |    PID    |  low
//|_______________________________|_______________________|

#define IS_VALID(x) ((x) & 0x00000001)
#define SET_VALID(x) ((x) | 0x00000001)
#define SET_PN(entry, pn) ((entry & 0x00000FFF) | (pn << 12))
#define GET_PN(entry) ((entry &~ 0x00000FFF) >> 12)
#define SET_NEXT(entry, next) ((entry & 0x00000FFF) | (next << 12))
#define GET_NEXT(entry) ((entry &~ 0x00000FFF) >> 12)
#define SET_PID(entry, pid) ((entry &~ 0x0000003F) | pid)
#define GET_PID(entry) (entry & 0x0000003F)
#define HAS_CHAIN(x) ((x) & 0x00000002)
#define SET_CHAIN(x) ((x) | 0x00000002)

struct PTE{
    uint32_t hi, low;
};

struct pT{
    struct PTE *entries;
    uint32_t size;
};

page_table pageTInit(uint32_t size){
    (void) size;
    return (page_table)0;
}

//Add a new entry into page table, set V, set next and chain bit to zero.
void addEntry(page_table pt, uint32_t page_n, uint32_t index, uint32_t pid){
    (void) pt;
    (void) page_n;
    (void) index;
    (void) pid;
    return;
}

//Return the index where page number is stored in, if page is not stored in memory, return -1
int getFrameN(page_table pt, uint32_t page_n, uint32_t pid){
    (void) pt;
    (void) page_n;
    (void) pid;
    return 0;
}

void setInvalid(page_table pt, uint32_t index){
    (void) pt;
    (void) index;
    return;
}

//Use kfree function
void pageTFree(page_table pt){
    (void) pt;
    return;
}
