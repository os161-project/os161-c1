#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <copyinout.h>
#include <vnode.h>
#include <vfs.h>
#include <limits.h>
#include <uio.h>
#include <proc.h>
#include <errno.h>
#include <current.h>

#define SYSTEM_OPEN_MAX (10*OPEN_MAX)


/* system open file table */
struct openfile {
  struct vnode *vn;
  off_t offset;	
  unsigned int countRef;
};

struct openfile systemFileTable[SYSTEM_OPEN_MAX];


int sys_read(int fd,char *buf, size_t size){
    int i;
    char *p=buf;
    if(fd!=STDIN_FILENO){
        kprintf("sys read implemented only on stdin\n");
        return -1;
    } 
    
    for(i=0;i<(int)size;i++){
        p[i]=getch();
        if(p[i]<0)
            return i;
        
    }
    return (int)size;
}

int sys_write(int fd,char *buf, size_t size){
    int i;
    
    if(fd!=STDOUT_FILENO && fd!=STDERR_FILENO){
        kprintf("sys write implemented only on stdout and stderr\n");
        return -1;
    }
    for(i=0;i<(int)size;i++){
        putch(buf[i]);
    }

    return (int)size;
}

int sys_open(userptr_t *path, int openflags, mode_t mode,int *errp){
    int fd,i,result;

    struct vnode *vn;
    struct openfile *of=NULL;

    result=vfs_open((char*)path,openflags,mode,&vn);
    if (result) {
        *errp = ENOENT;
        return -1;
    }

    /*search system open file table*/
    for(i=0;i<SYSTEM_OPEN_MAX;i++){
        if(systemFileTable[i].vn==NULL){
            of= &systemFileTable[i];
            of->vn=vn;
            of->countRef++;
            of->offset=0;
        }
    }
    if(of==NULL){
        // no free slot in system open file table
        *errp = ENFILE;
    }else{
        for(i=STDERR_FILENO+1;i<OPEN_MAX;i++){
            if(curproc->file_table[i]==NULL){
                curproc->file_table[i]=of;
                return fd;
            }
        }
        //if we are here means that we have no free slot in the process file table
        *errp=EMFILE;
    }
    vfs_close(vn);
    return -1;
}

int sys_close(int fp){
    struct open_file *of=NULL;
    struct vnode *v;

    if(fp<0 || fp>OPEN_MAX){
        return -1;
    }
    of=curproc->file_table[fp];
    if(of==NULL){
        return -1;
    }
    curproc->file_table[fp]=NULL;
    of->countRef--;
    if(of->countRef>0){
        //just decrement the occurrences of the opened file for this file
        return 0;
    }
    vn=of->vn;
    of->vn=NULL;
    if(vn==NULL){
        return -1;
    }
    vfs_close(vn);
    return 0;
}
