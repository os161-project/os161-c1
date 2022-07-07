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
//|       Virtual Page Number     |         |S|     PID   |  
//|_______________________________|_______________________|

#define IS_SWAPPED(x) ((x) & 0x00000040)
#define SET_SWAPPED(x, value) (((x) &~ 0x00000040) | (value << 6))
#define SET_PN(entry, pn) ((entry & 0x00000FFF) | (pn << 12))
#define GET_PN(entry) ((entry &~ 0x00000FFF) >> 12)
#define SET_PID(entry, pid) ((entry &~ 0x0000003F) | pid)
#define GET_PID(entry) (entry & 0x0000003F)

struct swapTable{
    struct vnode *fp;
    uint32_t *entries;
    uint32_t size;
};

swap_table swapTableInit(char swap_file_name[]){
    struct stat file_stat;
    swap_table result = kmalloc(sizeof(*result));
    int tmp = vfs_open(swap_file_name, O_RDWR, 0, &result->fp);
    if(tmp)
        panic("VM: Failed to create Swap area\n");
    VOP_STAT(result->fp, &file_stat);
    result->size = file_stat.st_size / PAGE_SIZE;
    result->entries = (uint32_t*)kmalloc(result->size * sizeof(*(result->entries)));
    for(uint32_t i = 0; i < result->size; i++){
        result->entries[i] = SET_SWAPPED(result->entries[i], 1);
    }
    return result;
}

void swapout(swap_table st, uint32_t index, paddr_t paddr, uint32_t page_number, uint32_t pid){
    int spl=splhigh();
    struct uio swap_uio;
    struct iovec iov;
    uio_kinit(&iov, &swap_uio, (void*)PADDR_TO_KVADDR(paddr & PAGE_FRAME), PAGE_SIZE, index*PAGE_SIZE, UIO_WRITE);
  

    // Add page into swap table
    st->entries[index] = SET_PN(SET_PID(SET_SWAPPED(st->entries[index], 0), pid), page_number);

    splx(spl);
    int result = VOP_WRITE(st->fp, &swap_uio);
    if(result)     
        panic("VM_SWAP_OUT: Failed");
    spl=splhigh();
    TLB_Invalidate(paddr);
    splx(spl);
}

void swapin(swap_table st, uint32_t index, paddr_t paddr){
    int spl=splhigh(), result;
    struct uio swap_uio;
    struct iovec iov;
    uio_kinit(&iov, &swap_uio, (void*)PADDR_TO_KVADDR(paddr & PAGE_FRAME), PAGE_SIZE, index*PAGE_SIZE, UIO_READ);

    // Remove page from swap table
    st->entries[index] = SET_SWAPPED(st->entries[index], 1);
    splx(spl);
    result=VOP_READ(st->fp, &swap_uio);

    if(result) 
        panic("VM: SWAPIN Failed");
}

int getFirstFreeChunckIndex(swap_table st){
    for(uint32_t i = 0; i < st->size; i++){
        if(IS_SWAPPED(st->entries[i]))
            return i;
    }
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
            st->entries[chunk_index] = SET_SWAPPED(SET_PID(SET_PN(st->entries[chunk_index], init_page_n), PID), 0);
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
        st->entries[chunk_index] = SET_SWAPPED(SET_PID(SET_PN(st->entries[chunk_index], init_page_n), PID), 0);
    }
    splx(spl);
}

int getSwapChunk(swap_table st, vaddr_t faultaddress, pid_t pid){
    uint32_t page_n = faultaddress >> 12;
    for(uint32_t i = 0; i < st->size; i++){
        if(GET_PN(st->entries[i]) == page_n && GET_PID(st->entries[i]) == (uint32_t)pid && !IS_SWAPPED(st->entries[i]))
            return i;
    }
    return -1;
}

void all_proc_chunk_out(swap_table st){
    for(uint32_t i = 0; i < st->size; i++){
        if(GET_PID(st->entries[i]) == (uint32_t)curproc->p_pid)
            st->entries[i] = SET_SWAPPED(st->entries[i], 1);
    }
}

void print_chunks(swap_table st){
    kprintf("\n");
    for(uint32_t i = 0; i < 5; i++){
        kprintf("Chunk %d : %x\n", i, st->entries[i]);
    }
}