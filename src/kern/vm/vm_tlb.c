#include "vm_tlb.h"

int TLB_Insert(vaddr_t faultaddress, paddr_t paddr){
    (void) faultaddress;
    (void) paddr;
    return 0;
}

int TLB_Invalidate_all(void){
    return 0;
}

int TLB_Invalidate(paddr_t paddr){
    (void) paddr;
    return 0;
}
