#include "swapfile.h"
#include "vm_tlb.h"
#include <uio.h>
#include <kern/stat.h>
#include <vm.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <spl.h>
#include <proc.h>
#include <current.h>

// S = Swapped bit
//<----------------20------------>|<----6-----><-----6--->|
//_________________________________________________________
//|       Virtual Page Number     |       |C|S|     PID   |  
//|_______________________________|_______________________|
//|                         Next                          |
//|_______________________________________________________|



#define IS_SWAPPED(x) ((x) & 0x00000040)
#define SET_SWAPPED(x, value) (((x) &~ 0x00000040) | (value << 6))
#define SET_PN(entry, pn) ((entry & 0x00000FFF) | (pn << 12))
#define GET_PN(entry) ((entry &~ 0x00000FFF) >> 12)
#define SET_PID(entry, pid) ((entry &~ 0x0000003F) | pid)
#define GET_PID(entry) (entry & 0x0000003F)
//the chain is used only for the free chunk list
#define HAS_CHAIN(x) ((x) & 0x00000080) 
#define SET_CHAIN(x, value) (((x) &~ 0x00000080) | (value << 7))
#define IS_FULL(st) (st->first_free_chunk == st->last_free_chunk && !IS_SWAPPED(st->entries[st->first_free_chunk].hi))


struct STE{
    uint32_t hi,low;
};

struct swapTable{
    struct vnode *fp;
    struct STE *entries;
    uint32_t size;
    uint32_t first_free_chunk;
    uint32_t last_free_chunk;
};

swap_table swapTableInit(char swap_file_name[]){
    struct stat file_stat;
    uint32_t i;
    swap_table result = kmalloc(sizeof(*result));
    int tmp = vfs_open(swap_file_name, O_RDWR, 0, &result->fp);
    if(tmp)
        panic("VM: Failed to create Swap area\n");
    VOP_STAT(result->fp, &file_stat);
    result->size = file_stat.st_size / PAGE_SIZE;
    result->entries = (struct STE*)kmalloc(result->size * sizeof(*(result->entries)));
    result->first_free_chunk = 0;
    for(i = 0; i < result->size - 1; i++){
        //the chain of free frames is initialized here
        result->entries[i].hi = SET_SWAPPED(SET_CHAIN(result->entries[i].hi,1), 1);
        result->entries[i].low = i+1;

    }
    //the last chunk has the chain bit set to 0
    result->entries[i].hi = SET_SWAPPED(SET_CHAIN(result->entries[i].hi,0), 1);
    result->entries[i].low = 0;
    result->last_free_chunk = i;
    //print_chunks(result);
    return result;
}

void swapout(swap_table st, uint32_t index, paddr_t paddr, uint32_t page_number, uint32_t pid, bool invalidate){
    int spl=splhigh();
    struct uio swap_uio;
    struct iovec iov;

    //update the first_free_chunk index
   
    delete_free_chunk(st, index);
    
    uio_kinit(&iov, &swap_uio, (void*)PADDR_TO_KVADDR(paddr & PAGE_FRAME), PAGE_SIZE, index*PAGE_SIZE, UIO_WRITE);
  

    // Add page into swap table
    st->entries[index].hi = SET_PN(SET_PID(SET_SWAPPED(st->entries[index].hi, 0), pid), page_number);

    splx(spl);
    int result = VOP_WRITE(st->fp, &swap_uio);
    if(result)     
        panic("VM_SWAP_OUT: Failed");
    spl=splhigh();
    if(invalidate)
        TLB_Invalidate(paddr);
    splx(spl);
}

void swapin(swap_table st, uint32_t index, paddr_t paddr){
    int spl=splhigh(), result;
    struct uio swap_uio;
    struct iovec iov;



    uio_kinit(&iov, &swap_uio, (void*)PADDR_TO_KVADDR(paddr & PAGE_FRAME), PAGE_SIZE, index*PAGE_SIZE, UIO_READ);

    // Remove page from swap table
    st->entries[index].hi = SET_SWAPPED(st->entries[index].hi, 1);
    splx(spl);
    result=VOP_READ(st->fp, &swap_uio);

    if(result) 
        panic("VM: SWAPIN Failed");
    
    //manage the free chunk list (add a free chunk)
    insert_into_free_chunk_list(st, index);
}

int getFirstFreeChunckIndex(swap_table st){
   /* for(uint32_t i = 0; i < st->size; i++){
        if(IS_SWAPPED(st->entries[i].hi))
            return i;
    }*/
    //kprintf("%d\n",!IS_FULL(st));
    if(!IS_FULL(st))
        return st->first_free_chunk;
    return -1;
}

void elf_to_swap(swap_table st, struct vnode *v, off_t offset, uint32_t init_page_n, size_t memsize, pid_t PID){
    int spl=splhigh();
    struct iovec iov_swap, iov_elf;
	struct uio ku_swap, ku_elf;
    char buffer[PAGE_SIZE / 2];
    int chunk_index, result, chunk_offset;
    uint32_t n_chuncks = (memsize + PAGE_SIZE - 1 - offset) / PAGE_SIZE, i, j, incr = PAGE_SIZE / 2;
    for(i = 0; i < n_chuncks - 1; i++, init_page_n++){
        // Get first chunck available
        chunk_index = getFirstFreeChunckIndex(st);
        if(chunk_index == -1){
            // Handle full swapfile
            panic("Swap area full!\n");
            return;
        }else{
            chunk_offset = chunk_index * PAGE_SIZE;
            for(j = 0; j < 2; j++, offset += incr, chunk_offset += incr){
                // Read one page from elf file
                uio_kinit(&iov_elf, &ku_elf, buffer, incr, offset, UIO_READ);
                result = VOP_READ(v, &ku_elf);
                if(result) 
                    panic("Failed loading elf into swap area!\n");

                // Write page into swapfile
                uio_kinit(&iov_swap, &ku_swap, buffer, incr, chunk_offset, UIO_WRITE);
                result = VOP_WRITE(st->fp, &ku_swap);
                if(result) 
                    panic("Failed loading elf into swap area!\n");
                
            }
            // Add page into swap table
            delete_free_chunk(st,chunk_index);
            st->entries[chunk_index].hi = SET_SWAPPED(SET_PID(SET_PN(st->entries[chunk_index].hi, init_page_n), PID), 0);
            //manage the free chunk list
            //since we have called the getFirstFreeChunkIndex we know that the st is not full yet
            
           
        }
    }
    chunk_index = getFirstFreeChunckIndex(st);
    if(chunk_index == -1){
        // Handle full swapfile
        panic("Swap area full!\n");
        return;
    }else{
        chunk_offset = chunk_index * PAGE_SIZE;
        if(memsize - offset > incr){
            // Read one page from elf file
            uio_kinit(&iov_elf, &ku_elf, buffer, incr, offset, UIO_READ);
            result = VOP_READ(v, &ku_elf);
            if(result) 
                panic("Failed loading elf into swap area!\n");

            // Write page into swapfile
            uio_kinit(&iov_swap, &ku_swap, buffer, incr, chunk_offset, UIO_WRITE);
            result = VOP_WRITE(st->fp, &ku_swap);
            if(result) 
                panic("Failed loading elf into swap area!\n");
            offset += incr;
            chunk_offset += incr;
        }
        uio_kinit(&iov_elf, &ku_elf, buffer, memsize - offset, offset, UIO_READ);
        result = VOP_READ(v, &ku_elf);
        if(result) 
            panic("Failed loading elf into swap area!\n");
        
        // Write page into swapfile
        uio_kinit(&iov_swap, &ku_swap, buffer, incr, chunk_offset, UIO_WRITE);
        result = VOP_WRITE(st->fp, &ku_swap);
        if(result) 
            panic("Failed loading elf into swap area!\n");
        // Add page into swap table
        delete_free_chunk(st, chunk_index);
        st->entries[chunk_index].hi = SET_SWAPPED(SET_PID(SET_PN(st->entries[chunk_index].hi, init_page_n), PID), 0);
         //manage the free chunk list
        //since we have called the getFirstFreeChunkIndex we know that the st is not full yet
        
       
    }
    //print_chunks(st);
    splx(spl);
}

int getSwapChunk(swap_table st, vaddr_t faultaddress, pid_t pid){
    uint32_t page_n = faultaddress >> 12;
    for(uint32_t i = 0; i < st->size; i++){
        if(GET_PN(st->entries[i].hi) == page_n && GET_PID(st->entries[i].hi) == (uint32_t)pid && !IS_SWAPPED(st->entries[i].hi))
            return i;
    }
    return -1;
}

void all_proc_chunk_out(swap_table st){
    for(uint32_t i = 0; i < st->size; i++){
        if(GET_PID(st->entries[i].hi) == (uint32_t)curproc->p_pid){
            st->entries[i].hi = SET_SWAPPED(st->entries[i].hi, 1);
            insert_into_free_chunk_list(st, i);
        }
    }

}

void chunks_fork(swap_table st, pid_t src_pid, pid_t dst_pid){
    uint32_t i, j;
    int free_chunk, result;
    char buffer[PAGE_SIZE / 2];
    struct uio swap_uio;
    struct iovec iov;
    uint32_t incr = PAGE_SIZE / 2, offset_src, offset_dst;
    for(i = 0; i < st->size; i++){
        if(GET_PID(st->entries[i].hi) == (uint32_t)src_pid && !IS_SWAPPED(st->entries[i].hi)){
            free_chunk = getFirstFreeChunckIndex(st);
            if(free_chunk == -1)
                panic("Wait...is swap area full?!\n");
            offset_src = i * PAGE_SIZE;
            offset_dst = free_chunk * PAGE_SIZE;
            for(j = 0; j < 2; j++, offset_src += incr, offset_dst += incr){
                //Reading from parent process chunk
                uio_kinit(&iov, &swap_uio, buffer, incr, offset_src, UIO_READ);
                result = VOP_READ(st->fp, &swap_uio);
                if(result) 
                    panic("Failed forking chunks!\n");
                
                //Writing new chunk for child process
                uio_kinit(&iov, &swap_uio, buffer, incr, offset_dst, UIO_WRITE);
                result = VOP_WRITE(st->fp, &swap_uio);
                if(result) 
                    panic("Failed forking chunks!\n");
            }
            delete_free_chunk(st, free_chunk);
            st->entries[free_chunk].hi = SET_PN(SET_PID(SET_SWAPPED(st->entries[free_chunk].hi, 0), dst_pid), GET_PN(st->entries[i].hi));
             //manage the free chunk list
            //since we have called the getFirstFreeChunkIndex we know that the st is not full yet
           
           
        }
    }
}

void print_chunks(swap_table st){
    kprintf("\n");
    for(uint32_t i = 0; i < 10; i++){
        kprintf("%d) : %x SWAPPED: %d  NEXT: %x\n", i, st->entries[i].hi, IS_SWAPPED(st->entries[i].hi), st->entries[i].low);
    }
    kprintf("last) : %x SWAPPED: %d  NEXT: %x\n" , st->entries[st->last_free_chunk].hi, IS_SWAPPED(st->entries[st->last_free_chunk].hi),st->entries[st->last_free_chunk].low);
}

void checkDuplicatedEntries(swap_table st){
    uint32_t i, j, first_pn, first_pid, second_pn, second_pid;
    for(i = 0; i < st->size; i++){
        first_pn = GET_PN(st->entries[i].hi);
        first_pid = GET_PID(st->entries[i].hi);
        for(j = 0; j < st->size; j++){
            if(i != j){
                second_pn = GET_PN(st->entries[j].hi);
                second_pid = GET_PID(st->entries[j].hi);
                if(first_pn == second_pn && first_pid == second_pid){
                    kprintf("\nDuplicated entries!\nFirst at %d: 0x%x\nSecond at %d: 0x%x\n", i, st->entries[i].hi, j, st->entries[j].hi);
                    return;
                }
            }
        }
    }
    kprintf("\nNo duplicated entries!\n");
    return;
}


void 
delete_free_chunk(swap_table st,uint32_t chunk_to_delete){
    if(st->first_free_chunk == chunk_to_delete)
        st->first_free_chunk = st->entries[st->first_free_chunk].low;
    else{
        uint32_t i;
        //go to the index through the free list
        for(i = st->first_free_chunk; HAS_CHAIN(st->entries[i].hi) && st->entries[i].low != chunk_to_delete; i = st->entries[i].low);
        if(i == st->last_free_chunk)
            panic("Maybe we forgot to add the frame to the free list!\n");
        if(chunk_to_delete == st->last_free_chunk){
            st->last_free_chunk = i;
            //the last has no chain
            st->entries[i].hi = SET_CHAIN(st->entries[i].hi,0);
        }else{
            st->entries[i].low = st->entries[chunk_to_delete].low;
        }
    }
    
}

void insert_into_free_chunk_list(swap_table st, uint32_t chunk_to_add){
    if(IS_FULL(st)){
        st->first_free_chunk = st->last_free_chunk = chunk_to_add;
        st->entries[chunk_to_add].hi = SET_CHAIN(st->entries[chunk_to_add].hi, 0);

    }else{
        st->entries[st->last_free_chunk].hi = SET_CHAIN(st->entries[st->last_free_chunk].hi,1);
        st->entries[st->last_free_chunk].low = chunk_to_add;
        st->last_free_chunk = chunk_to_add;
        st->entries[st->last_free_chunk].low = 0;
        st->entries[st->last_free_chunk].hi = SET_CHAIN(st->entries[st->last_free_chunk].hi,0);
    }
}