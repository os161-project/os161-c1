/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <opt-paging.h>
#include <pt.h>
#include <types.h>
#include <mainbus.h>
#include "vm_tlb.h"

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 *
 * NOTE: it's been found over the years that students often begin on
 * the VM assignment by copying dumbvm.c and trying to improve it.
 * This is not recommended. dumbvm is (more or less intentionally) not
 * a good design reference. The first recommendation would be: do not
 * look at dumbvm at all. The second recommendation would be: if you
 * do, be sure to review it from the perspective of comparing it to
 * what a VM system is supposed to do, and understanding what corners
 * it's cutting (there are many) and why, and more importantly, how.
 */

/* under dumbvm, always have 72k of user stack */
/* (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define DUMBVM_STACKPAGES    18

/*
 * Wrap ram_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

page_table IPT;

void
vm_bootstrap(void)
{
	vm_enabled = 0;
	// Swap area init
	char swap_file_name[] = "lhd0raw:";
	ST = swapTableInit(swap_file_name);
	// IPT init
	size_t ram_size = mainbus_ramsize();
	spinlock_acquire(&stealmem_lock);
	paddr_t ram_user_base = ram_stealmem(0);
	spinlock_release(&stealmem_lock);
	IPT = pageTInit((ram_size-ram_user_base-PAGE_SIZE)/PAGE_SIZE);
	vm_enabled = 1;

}

/*
 * Check if we're in a context that can sleep. While most of the
 * operations in dumbvm don't in fact sleep, in a real VM system many
 * of them would. In those, assert that sleeping is ok. This helps
 * avoid the situation where syscall-layer code that works ok with
 * dumbvm starts blowing up during the VM assignment.
 */
static
void
dumbvm_can_sleep(void)
{
	if (CURCPU_EXISTS()) {
		/* must not hold spinlocks */
		KASSERT(curcpu->c_spinlocks == 0);

		/* must not be in an interrupt handler */
		KASSERT(curthread->t_in_interrupt == 0);
	}
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	paddr_t pa;

	dumbvm_can_sleep();
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */

	(void)addr;
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	//vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	int paddr;
	//uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
			//try to access a read-only segment causes a fault, terminate the process
			//TO-DO: terminate the process
			break;
	    case VM_FAULT_READ:
			
			break;
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	//KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	//KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	//KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	//KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	//KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	//KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
	/*
	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}*/

	/* make sure it's page-aligned */
	//KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	if(faultaddress <=MIPS_KSEG0){

		//retrieve the frame number in the page table
		paddr = getFrameN(IPT,(faultaddress & PAGE_FRAME) >> 12);
		if(paddr==-1){
			//PAGE FAULT
			//TO_DO: handle page fault
			paddr = handle_page_fault(faultaddress);
		}
		//add to tlb
		paddr = paddr & PAGE_FRAME;
		TLB_Insert(faultaddress,paddr);

		
		splx(spl);
		return 0;
	}

	return EFAULT;
}

uint32_t handle_page_fault(vaddr_t faultaddress){
	uint32_t paddr_t;
	//TO-DO: here we need to load a page from disk into memory
	(void)paddr_t;
	uint32_t victim_frame;
	int chunk_index = getSwapChunk(ST, faultaddress), free_chunk_index;
	if(chunk_index == -1){
		panic("Unavailable chunk in swap file!\nThis shouldn't happen...\n");
	}
	// We have to implement a search for free frames, now I'm assuming the IPT is full...
	victim_frame = replace_page(IPT);
	free_chunk_index = getFirstFreeChunckIndex(ST);
	if(free_chunk_index == -1){
		panic("Wait...is swap area full?!");
	}
	// As I said...I'm covering the replacement case, if we have a free frame it's not necessary
	swapout(ST, free_chunk_index, victim_frame << 12, IPT);
	swapin(ST, chunk_index, victim_frame << 12, IPT/*, faultaddress*/);
	return 0;
}