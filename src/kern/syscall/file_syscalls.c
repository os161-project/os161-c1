#include <kern/unistd.h>
#include <types.h>
#include <lib.h>
#include <syscall.h>

/*
*   Prototypes:
*    -> ssize_t read(int filehandle, void *buf, size_t size);
*    -> ssize_t write(int filehandle, const void *buf, size_t size);
*/

uint32_t sys_write(int fd, const void* buf, size_t size) {
    if(fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        uint32_t i;
        for(i = 0; i < size; i++) {
            putch(((char*)buf)[i]);
        }
        return size;
    } else {
        kprintf("Explicit management of files is not yet implemented.");
        return -1; 
    }
}

uint32_t sys_read(int fd, void* buf, size_t size) {
    if(fd == STDIN_FILENO) {
        uint32_t i; int c;
        for(i = 0; i < size; i++) {
            c = getch();
            if(c < 0) return i; 
            ((char*)buf)[i] = c;
        }
        ((char*)buf)[i] = '\0';
        return size;
    } else {
        kprintf("Explicit management of files is not yet implemented.");
        return -1;
    }
}