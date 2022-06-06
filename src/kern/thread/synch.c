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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#ifndef OPT_SEM_LOCK
#include "opt-sem_lock.h"
#endif
#ifndef OPT_WCHAN_LOCK
#include "opt-wchan_lock.h"
#endif
#ifndef OPT_CV
#include "opt-cv.h"
#endif

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
        struct semaphore *sem;

        sem = kmalloc(sizeof(*sem));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(*lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);
        lock->me = (struct spinlock *) kmalloc(sizeof(struct spinlock));
        KASSERT(lock->me != NULL);
        spinlock_init(lock->me);
#if OPT_SEM_LOCK
        spinlock_init(lock->me);
        lock->sem = (struct semaphore *) kmalloc(sizeof(struct semaphore));
        KASSERT(lock->sem != NULL);
        lock->sem = sem_create("lock_sem", 1);
        lock->counter = 1;
#elif OPT_WCHAN_LOCK
        lock->wch = wchan_create("lock_wchan");
        KASSERT(lock->wch != NULL);
        lock->counter = 1; 
#endif
        lock->owner = NULL;
        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);
        spinlock_acquire(lock->me);
        kfree(lock->owner);
        spinlock_release(lock->me);
#if OPT_SEM_LOCK
        sem_destroy(lock->sem);
        // sem is freed inside sem_destroy, so no memory leaks here
#elif OPT_WCHAN_LOCK
        wchan_destroy(lock->wch);
#endif
        spinlock_cleanup(lock->me);
        // but spinlock_cleanup behavior is different, so I have to free 
        // mutual exclusion spinlock in order to avoid memory leaks.
        kfree(lock->me);
        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        KASSERT(lock != NULL);
	/* Call this (atomically) before waiting for a lock */
	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);
        spinlock_acquire(lock->me);
        while(lock->counter == 0) {
#if OPT_SEM_LOCK
                // Sleep until we can (possibly) acquire the lock
                spinlock_release(lock->me);
                P(lock->sem);  
                spinlock_acquire(lock->me);
#elif OPT_WCHAN_LOCK
                wchan_sleep(lock->wch, lock->me);
#endif
        }
        KASSERT(lock->counter == 1);
        KASSERT(lock->owner == (struct thread*) NULL);
        lock->counter--; 
        lock->owner = curthread;
        spinlock_release(lock->me);
	/* Call this (atomically) once the lock is acquired */
	HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);
}

void
lock_release(struct lock *lock)
{
        KASSERT(lock != NULL);
	/* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);
        KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(lock->me);
        KASSERT(lock->counter == 0); // If we want that releasing a lock not previously acquired generates an error
        lock->counter++;
        KASSERT(lock->counter == 1);
#if OPT_SEM_LOCK
	V(lock->sem);
#elif OPT_WCHAN_LOCK
        wchan_wakeone(lock->wch, lock->me);
#endif
        lock->owner = (struct thread*) NULL;
	spinlock_release(lock->me);
}

bool
lock_do_i_hold(struct lock *lock)
{
        KASSERT(lock != NULL);
        bool result = false;
        // Write this
        spinlock_acquire(lock->me);
        if(lock->owner == curthread) {
                result = true;
        } 
        spinlock_release(lock->me);
        return result;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(*cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
#if OPT_CV
        cv->wch = wchan_create("cv_wchan");
        cv->slk = (struct spinlock *) kmalloc(sizeof(struct spinlock));
        KASSERT(cv->slk != NULL);
        spinlock_init(cv->slk);
#else 
#endif
        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);
#if OPT_CV
        wchan_destroy(cv->wch);
        spinlock_cleanup(cv->slk);
        kfree(cv->slk);
#else
#endif
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
        KASSERT(cv != NULL && lock != NULL);
#if OPT_CV
        KASSERT(lock_do_i_hold(lock));
        // The spinlock for mutual exclusion must be acquired before releasing the
        // lock. Otherwise we could have a deadlock condition: when releasing the lock
        // we must be sure we can go to sleep. So, we need to first acquire the spinlock.
        spinlock_acquire(cv->slk); // This is needed for avoiding that another thread acquires the lock before the current thread goes to sleep on wchan_sleep.
        lock_release(lock);
        wchan_sleep(cv->wch, cv->slk);
        // slk lock must be released before calling lock_acquire, 
        // otherwise we will go to sleep (inside lock_acquire), having already acquire two spinlocks,
        // which will 'cause a kernel error.
        spinlock_release(cv->slk);
        lock_acquire(lock);
#else
        (void)cv;    // suppress warning until code gets written
        (void)lock;  // suppress warning until code gets written
#endif
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
        KASSERT(cv != NULL && lock != NULL);
#if OPT_CV
        KASSERT(lock_do_i_hold(lock));
        spinlock_acquire(cv->slk);
        wchan_wakeone(cv->wch, cv->slk);
        spinlock_release(cv->slk);
#else
        (void)cv;    // suppress warning until code gets written
        (void)lock;  // suppress warning until code gets written
#endif
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
        // Write this
        KASSERT(cv != NULL && lock != NULL);
#if OPT_CV
        KASSERT(lock_do_i_hold(lock));
        spinlock_acquire(cv->slk);
        wchan_wakeall(cv->wch, cv->slk);
        spinlock_release(cv->slk);
#else
        (void)cv;    // suppress warning until code gets written
        (void)lock;  // suppress warning until code gets written
#endif
}
