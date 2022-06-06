#ifndef __TLB_H__
#define __TLB_H__
#include <types.h>
//These functions are wrappers for the low level TLB mips assembly implementation
int TLB_Invalidate_all();
int TLB_Invalidate(paddr_t paddr);
int TLB_Insert(vaddr_t faultaddress, paddr_t paddr);


#endif

