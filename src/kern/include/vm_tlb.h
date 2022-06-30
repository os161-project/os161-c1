#ifndef __TLB_H__
#define __TLB_H__
#include <types.h>
#include <stdlib.h>>
#include <stdio.h>
//These functions are wrappers for the low level TLB mips assembly implementation
int TLB_Invalidate_all(void);
int TLB_Invalidate(paddr_t paddr);
int TLB_Insert(vaddr_t faultaddress, paddr_t paddr);
int tlb_get_rr_victim(void);
int is_code_segment(vaddr_t vaddr);


#endif

