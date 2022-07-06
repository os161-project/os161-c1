#include "pt.h"
#include <types.h>
#include <current.h>
#include <proc.h>
#include <vm.h>
#include <spl.h>

// V = validity bit
// C = chain bit (if next field has a value)
//<----------------20------------>|<----6-----><----6---->|
//_________________________________________________________
//|       Virtual Page Number     |                 |K|C|V|  hi
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
#define IS_KERNEL(x) ((x) & 0x00000004)
#define SET_KERNEL(x, value) (((x) &~ 0x00000004) | (value << 2))

struct PTE{
    uint32_t hi, low;
};

struct pT{
    struct PTE *entries;
    uint32_t size;
    //free frames are managed as a list of pages for a process
    //so we have a chain of free frame, first_free_frame is the index of the first free frame of the list
    //the next free frame is the one indexed by the next field in the low part
    uint32_t first_free_frame;
    bool is_full;
    int num_occupied_entries;
};

page_table pageTInit(uint32_t n_pages){
    uint32_t i;
    page_table tmp = kmalloc(sizeof(*tmp));
    tmp->entries = kmalloc(n_pages * sizeof(*(tmp->entries)));
    tmp->size = n_pages;
    tmp->first_free_frame=0;
    tmp->is_full=false;
    tmp->num_occupied_entries=0;
    for(i = 0; i < n_pages-1; i++){
        //the chain of free frames is built here
        //when the ipt is initialized the list of free frames includes all the frames
        tmp->entries[i].hi = SET_PN(SET_VALID(SET_CHAIN(tmp->entries[i].hi, 1), 0), 0);
        tmp->entries[i].low = SET_NEXT(SET_PID(tmp->entries[i].low, 0), (i+1));
        
    }
    //the last frame has the chain bit set to 0 (no chain) 
    tmp->entries[i].hi= SET_PN(SET_VALID(SET_CHAIN(tmp->entries[i].hi,0),0),0);
    tmp->entries[i].low = SET_NEXT(SET_PID(tmp->entries[i].low, 0), 0);
    return tmp;
}

//Add a new entry into page table, set V, set next and chain bit to zero.
void addEntry(page_table pt, uint32_t page_n, uint32_t index, uint32_t pid){
    
    if((page_n << 12)> MIPS_KSEG0){
        //set the frame as part of the kernel
        pt->entries[index].hi = SET_PN(SET_CHAIN(SET_VALID(SET_KERNEL(pt->entries[index].hi, 1),1), 0), page_n);
        pt->entries[index].low = SET_NEXT(pt->entries[index].low, 0);
    }else{
        //set the frame as not part of the kernel
        pt->entries[index].hi = SET_PN(SET_CHAIN(SET_VALID(SET_KERNEL(pt->entries[index].hi, 0),1), 0), page_n);
        pt->entries[index].low = SET_PID(SET_NEXT(pt->entries[index].low, 0),pid);
    }

    pt->num_occupied_entries++;
    if(pt->num_occupied_entries == pt->size)
        pt->is_full=true;
    
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
    uint32_t page_index;

    do{
        page_index = random() % pt->size;
    }while(IS_KERNEL(pt->entries[page_index].hi));

    splx(spl);
    return page_index;
}

 void add_to_chain(page_table pt){


    if(curproc->last_pt_i==-1){
        //we need to update also the head of the chain
        curproc->start_pt_i = pt->first_free_frame;
        curproc->last_pt_i=pt->first_free_frame;
        return;
    }
    //the penultimate frame of the chain is updated
    //the chain is updated and the next field is updated indexing the last frame
    pt->entries[curproc->last_pt_i].hi = SET_CHAIN(pt->entries[curproc->last_pt_i].hi, 1);
    pt->entries[curproc->last_pt_i].low = SET_NEXT(pt->entries[curproc->last_pt_i].low, pt->first_free_frame);

    curproc->last_pt_i=pt->first_free_frame;

 }


paddr_t pageIn(page_table pt, uint32_t pid, vaddr_t vaddr, swap_table ST) {
    int spl;
    paddr_t paddr, page_index;
    int chunk_index, free_chunk_index;

    spl = splhigh();
    
    if(!pt->is_full){
        //if the ipt is not full
       // add an entry in the first free frame of the ipt
        addEntry(pt, (vaddr & PAGE_FRAME) >> 12,  pt->first_free_frame, curproc->p_pid);

        //add the new entry to the chain of the process
        add_to_chain(pt);

        //update the first free frame index
        paddr= pt->first_free_frame * PAGE_SIZE;
        pt->first_free_frame = GET_NEXT(pt->entries[pt->first_free_frame].low); 
        //load a frame in memory
        chunk_index = getSwapChunk(ST, vaddr); // add pid
        if(chunk_index == -1){
		    panic("Unavailable chunk in swap file!\nThis shouldn't happen...\n");
	    }
        swapin(ST, chunk_index, paddr, vaddr);
        return paddr; 
    }

    //if we are here the ipt is full
    page_index=replace_page(pt);

    addEntry(pt, (vaddr & PAGE_FRAME), page_index, curproc->p_pid);

    paddr= page_index * PAGE_SIZE;

    chunk_index = getSwapChunk(ST, vaddr);
	if(chunk_index == -1){
		panic("Unavailable chunk in swap file!\nThis shouldn't happen...\n");
	}

    free_chunk_index = getFirstFreeChunckIndex(ST);
	if(free_chunk_index == -1){
		panic("Wait...is swap area full?!");
	}

	swapout(ST, free_chunk_index, paddr, (vaddr & PAGE_FRAME));
	swapin(ST, chunk_index, paddr, vaddr);

    return paddr;

    // TODO: Load the frame directly to the address space of the process

    
    // TODO: Set vpage number 
    // Maybe we can call addEntry function and modify it
    /*pt->entries[index].low = SET_PID(pt->entries[index].low, pid);
    pt->entries[index].hi = SET_VALID(SET_CHAIN(pt->entries[index].hi, 0), 1);*/
   
    (void)pid;
    splx(spl);
}

void  all_proc_page_out(page_table pt){
    //invalidate all the pages of the process
     for(int i = curproc->start_pt_i; i != -1 && HAS_CHAIN(pt->entries[i].hi); i = GET_NEXT(pt->entries[i].low)){
        SET_VALID(pt->entries[i].hi,0);
    }
}