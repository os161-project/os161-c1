#include <kern/unistd.h>
#include <types.h>
#include <lib.h>
#include <proc.h>
#include <syscall.h>
#include <thread.h>
#include <addrspace.h>
#include <current.h>
#include <kern/errno.h>
#include <mips/trapframe.h>

void sys__exit(int status)
{

    /*
     *   This is needed, because proc_remthread(curthread) is setting curthread->t_proc to NULL
     *   for detaching the thread, but curproc is just a macro for curthread->t_proc (see how it expands).
     */
    struct proc *curp = curproc;
    // Setting the termination (exit) status code
    curp->p_exitcode = status & 0xff; /* Just lower 8 bits are returned */
    // Signaling on the proc semaphore, in order to wake up another process waiting on a
    // proc_wait call.
    /*
     *  IMPORTANT NOTE:
     *  By calling here the function proc_remthread(), we're avoiding a possible race condition,
     *  happening due to the fact that when we signal on the proc semaphore, we still have to call a thread_exit
     *  function. So, it's possible that when we call (on the proc_wait side) proc_destroy before calling thread_exit
     *  with the check KASSERT(proc->p_numthreads == 0) (see proc_destroy implementation) failing because of this reason.
     */
    /*
     *  What the function proc_remthread basically does it setting the process reference of the current thread to null and
     *  decrementing the number of threads of the same process.
     *  Also remember this: a thread has a reference to its process, while a process doesn't have any reference to its threads
     *  (just a number represeting the total counter of active (i.e. not detached) threads).
     */
    proc_remthread(curthread); // Detaching myself before signaling
    // Additional note: the thread_exit function is also modified according to the previous call
    V(curp->wp_sem);
    // Putting the thread in a zombie state
    // Thread descriptor will actually be destroyed by the call to thread_switch,
    // destroying the zombie thread.
    thread_exit();

    panic("thread_exit should never return\n");
}

pid_t sys_waitpid(pid_t pid, userptr_t statusp, int options)
{
    struct proc *p = proc_getproc(pid);
    int status;
    (void)options; /* Not handled in this implementation */
    if (p == NULL)
        return -1;
    status = proc_wait(p);
    if (statusp != NULL)
    {
        *(int *)statusp = status;
    }
    return pid; // return pid on success
}

pid_t 
sys_getpid(void)
{
    if (curproc == NULL)
    {
        return -1;
    }
    return proc_getpid(curproc);
}

/* Just a wrapper function, required by the thread_fork call */
static void
call_enter_forked_process(void *tfv, unsigned long dummy) {
    struct trapframe *tf = (struct trapframe*) tfv;
    (void) dummy;
    enter_forked_process(tf);
    panic("enter_forked_process should never return :(!");
}

int 
sys_fork(struct trapframe *ctf, pid_t *retval)
{
    /* I need to create another process, starting from the current one */
    struct trapframe *tf_child;
    struct proc *newp;
    int result;

    KASSERT(curproc != NULL);

    newp = proc_create_runprogram(curproc->p_name);
    if (newp == NULL)
    {
        return ENOMEM;
    }

    /* Then, I need to duplicate the address space */
    as_copy(curproc->p_addrspace, &newp->p_addrspace);
    if (newp->p_addrspace == NULL)
    {
        proc_destroy(newp);
        return ENOMEM;
    }

    /* We need a copy of the entire parent's trapframe */
    tf_child = kmalloc(sizeof(struct trapframe));
    if (tf_child == NULL)
    {
        proc_destroy(newp);
        return ENOMEM;
    }
    memcpy(tf_child, ctf, sizeof(struct trapframe));

    /*
     *  At this point we can call a thread_fork, linking the new thread
     *  with the newly created process.
     */

    result = thread_fork(newp->p_name, 
                        newp, 
                        call_enter_forked_process,
                        (void*) tf_child,
                        (unsigned long) 0 /*not used*/);
    
    if(result) {
        proc_destroy(newp);
        kfree(tf_child);
        return ENOMEM;
    }
    *retval = newp->p_pid;
    return 0;
}