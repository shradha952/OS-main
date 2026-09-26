// important header files
// shared memory functions
// key is an identifier for a shared memory page
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"

#define TPG 512

struct pages_alloc{ // struct holding physical address - pa - first address of the page, lk
    struct spinlock lk;
    uint64 pa;
};

struct pages_alloc key2pa[TPG];//mapping key to struct above - using index of the array
uint ref[TPG]; // how many processes are referring to the page
uint sema[TPG]; // semaphore , to access the sahred memory, binary semaphore





uint64
shm_init(){ //initialisation
    for(int i = 0; i<TPG; i++){
        initlock(&(key2pa[i].lk), "key2palk");
        key2pa[i].pa = -1;
        sema[i] = 0;
    }

    return 0;

}

uint64
accesss(int k){ // based on semaphore- 
    struct pages_alloc * pga = &key2pa[k];
    acquire(&(pga->lk));
    while(sema[k] != 0){
        sleep(pga, &(pga->lk));
    }
    sema[k] = 1;
    release(&(pga->lk));

    return 0;
}

uint64
signals(int k){ //release access
    struct pages_alloc * pga = &key2pa[k];
    acquire(&(pga->lk));
    while(sema[k] == 0){
        sleep(pga, &(pga->lk));
    }
    sema[k] = 0;
    wakeup(pga);
    release(&(pga->lk));
    return 0;
}


uint64
create_shm(int k){

    acquire(&(key2pa[k].lk));
    if(key2pa[k].pa != -1){
        printf("already exist!!!!\n");
        release(&(key2pa[k].lk));
        struct proc *p = myproc();
        return p->key2va[k];
    }
    else{
        void *ptr = kalloc(); // physical page allocate
        if(!ptr){
            printf("helllo1\n");
            panic("kalloc failed");
        }
        
        memset(ptr, 0, PGSIZE);
        key2pa[k].pa = (uint64)ptr;
        struct proc * p= myproc();

        uint64 va = (p->sz);

        if(mappages(p->pagetable, va, PGSIZE, key2pa[k].pa, PTE_R | PTE_W | PTE_U | PTE_S) != 0){ //mapping pa <- va 
            kfree(ptr); // free the page 
            panic("mempage failed");
        }
        p->sz += PGSIZE;
        p->key2va[k] = va; // proc.h - struct proc - key to va first va
        ref[k]++;
        release(&(key2pa[k].lk));
        printf("hello2 viratual address:::: %lu\n", va);
        return va;
    }
}

uint64 
get_shm(int k){
    struct proc * p= myproc();
    acquire(&(key2pa[k].lk)); // to avoid race condition - deal with simultaneous accesses
    if(p->key2va[k] != -1){
        release(&(key2pa[k].lk));
        return p->key2va[k];
    }
        
        uint64 va = (p->sz);

        if(mappages(p->pagetable, va, PGSIZE, key2pa[k].pa, PTE_R | PTE_W | PTE_U) != 0){
            printf("mappages failed");
            return 0;
        }
        p->sz += PGSIZE;
        p->key2va[k] = va;
        ref[k]++;
        
        release(&(key2pa[k].lk));
        return va;
}


int close_shm(int k){ // first unmap va to pa mapping, if last mapping dellocate page

    struct proc *p = myproc();
    acquire(&(key2pa[k].lk));
    if(p->key2va[k] == -1){
        printf("process have no shared memory with this key");
        return -1;
    }
    uint64 va = p->key2va[k];
    uvmunmap(p->pagetable, va , 1, 0);

    p->key2va[k] = -1;
    p->sz -= PGSIZE;

    if(ref[k] > 0) --ref[k];
    
    if(ref[k] == 0 && key2pa[k].pa != (uint64)-1){
        kfree((void *)key2pa[k].pa); // deallocate
        key2pa[k].pa = -1;
    } 
    release(&(key2pa[k].lk));
    return 0;

}


