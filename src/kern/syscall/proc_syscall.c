#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <current.h>

void sys_exit(int status){
    

    struct proc *p=curproc;
    p->exit_status=status;
    //remove the thread
    proc_remthread(curthread);
    //signal
    V(p->sem);

    thread_exit(); 
    
    //we should never arrive here, the thread shoud not return
    panic("thread_exit returned (should not happen)\n");
    (void) status; // TODO: status handling
}

int sys_waitpid(pid_t pid,userptr_t statusp, int option){
    struct proc *p=proc_search_pid(pid);
    (void) option;
    int s;
    s=proc_wait(curproc);
    if(statusp!=NULL)
        *(int*)statusp=s;
    return pid;
}

pid_t sys_getpid(void){
    KASSERT(curproc!=NULL);
    return curproc->p_pid;
}