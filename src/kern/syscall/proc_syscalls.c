#include <kern/unistd.h>
#include <types.h>
#include <lib.h>
#include <proc.h>
#include <syscall.h>
#include <thread.h>
#include <addrspace.h>


int sys__exit(int status) {
    // Handling of the termination status is not done in this basic implementation
    // Destroying current process address space, after fetching it with proc_getas()
    as_destroy(proc_getas());
    // Putting the thread in a zombie state
    // Thread descriptor will actually be destroyed by the call to thread_switch,
    // destroying the zombie thread.
    thread_exit();
    return status;
}