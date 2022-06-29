#include "vm_tlb.h"
#include <mips/tlb.h>
#include <spl.h>

int TLB_Insert(vaddr_t faultaddress, paddr_t paddr){
    (void) faultaddress;
    (void) paddr;
    return 0;
}

int TLB_Invalidate_all(void){
	// Code to invalidate here
	int i;
	int spl;
	
	//u_int32_t ehi, elo;
	//DEBUG(DB_VM, "Invalidating TLB.\n");
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
    return 0;
}

int TLB_Invalidate(paddr_t paddr){
    (void) paddr;
    return 0;
}
