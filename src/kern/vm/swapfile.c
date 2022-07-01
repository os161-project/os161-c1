#include "swapfile.h"
#include <vnode.h>
#include <uio.h>
#include <kern/stat.h>
#include <vm.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <spl.h>

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
    int size;
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

void swapout(swap_table st, uint32_t index, paddr_t paddr, page_table pt){
    int spl=splhigh();
    struct uio swap_uio;
    struct iovec iov;
    uint32_t index_pt, page_number, PID;
    uio_kinit(&iov, &swap_uio, (void*)PADDR_TO_KVADDR(paddr & PAGE_FRAME), PAGE_SIZE, index*PAGE_SIZE, UIO_WRITE);
    index_pt = (paddr & PAGE_FRAME) >> 12;
    page_number = getPageN(pt, index_pt);
    PID = getPID(pt, index_pt);

    // Add page into swap table
    st->entries[index] = SET_PN(st->entries[index], page_number) | SET_PID(st->entries[index], PID) | SET_SWAPPED(st->entries[index], 0);

    splx(spl);
    int result = VOP_WRITE(st->fp, &swap_uio);
    if(result)     
        panic("VM_SWAP_OUT: Failed");
    spl=splhigh();
    TLB_Invalidate(paddr);
    splx(spl);
}

void swapin(swap_table st, uint32_t index, paddr_t paddr){
    int spl=splhigh();
    struct uio swap_uio;
    struct iovec iov;
    uint32_t index_pt, page_number, PID;
    uio_kinit(&iov, &swap_uio, (void*)PADDR_TO_KVADDR(paddr & PAGE_FRAME), PAGE_SIZE, index*PAGE_SIZE, UIO_READ);

    // Remove page from swap table
    st->entries[index] = SET_SWAPPED(st->entries[index], 1);

    splx(spl);
    int result=VOP_READ(st->fp, &swap_uio);
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
