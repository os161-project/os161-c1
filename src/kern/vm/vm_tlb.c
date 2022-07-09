#include "vm_tlb.h"
#include <mips/tlb.h>
#include <spl.h>
#include <addrspace.h>
#include <proc.h>
#include <vmstats.h>

int TLB_Insert(vaddr_t faultaddress, paddr_t paddr){
	int i,spl;
	uint32_t hi,lo;

	//disable interrupt
	spl=splhigh();

	int is_code_seg= is_code_segment(faultaddress);

	//scan the tlb in order to find a free entry (invalid)
	for(i=0;i<NUM_TLB;i++){
		tlb_read(&hi,&lo,i);
		if(!(lo & TLBLO_VALID)){
			//free entry found
			hi=faultaddress;
			//if it's a code segment we set it as read_only
			if(is_code_seg==1){
				lo=paddr | TLBLO_VALID;
			}else{
				lo=paddr | TLBLO_DIRTY | TLBLO_VALID;
			}
			/* statistics */ add_TLB_fault_type(TLB_FREE);
			tlb_write(hi,lo,i);
			//enable interrupt
			splx(spl);
			return 0;
		}
	}

	//if we are here we need to use a replacement algorithm (TLB full)
	//choose the victim
	int victim=tlb_get_rr_victim();
	/* statistics */ add_TLB_fault_type(TLB_REPLACE);
	//write in the tlb at index = victim
	hi=faultaddress;
	lo=paddr | TLBLO_DIRTY | TLBLO_VALID;
	tlb_write(hi,lo,victim);

    splx(spl);
    return 0;
}

int tlb_get_rr_victim(void) {
	int victim;
	static unsigned int next_victim = 0;
	victim = next_victim;
	next_victim = (next_victim + 1) % NUM_TLB;
	return victim;
}

int is_code_segment(vaddr_t vaddr){
	struct addrspace *as;

	as = proc_getas();
	//get the first page number
	uint32_t code_first_pn= as->as_vbase1;
	int num_pages=as->as_npages1;

	//get the last page number
	uint32_t code_last_pn= as->as_vbase1 + (num_pages*PAGE_SIZE);

	if(vaddr >= code_first_pn && vaddr <= code_last_pn )
		return 1; //it's a code segment
	return 0;
}

int TLB_Invalidate_all(void){
	// Code to invalidate here
	int i;
	int spl;
	spl = splhigh();
	/* statistics */ add_TLB_invalidation();
	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
    return 0;
}

int TLB_Invalidate(paddr_t paddr){
    
	int i;
	uint32_t hi,lo,frame_number;
	//retrieve the frame number (physical address without offset)
	frame_number=paddr & TLBLO_PPAGE;
	for(i=0;i<NUM_TLB;i++){
		tlb_read(&hi,&lo,i);
		if(frame_number== (lo & TLBLO_PPAGE)){
			tlb_write(TLBHI_INVALID(i),TLBLO_INVALID(),i);
		}
	}

    return 0;
}