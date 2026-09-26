
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "mru.h"
#include "fcntl.h"
#include "stat.h"





void update_mru(struct proc* p){
    if(!p) return;
    struct pglist* pg;
    pte_t *pte;
    
    for(pg = p->freepg; pg<&p->freepg[MAX_DPG]; pg++){
        if(pg->used){
            pte = walk(p->pagetable, pg->va, 0);
            if(*pte & PTE_A){
                p->head = pg;
                *pte = (*pte & ~PTE_A);
                printf("ok babe accessed!!\n");
            }

        }
    }
    
    sfence_vma();

}
// void create_swap_file(struct proc* );
// void remove_swap_file(struct proc* );
// int swap_out(struct proc *); //return block no address
// int swap_in(struct proc* , uint64);

// int add_to_mru(struct proc*, uint64 va, uint64 pa, int);
// int remove_pa_from_mru(struct proc* , uint64 );


// int write_swapfile(struct proc* , uint64 );
// // int mark_va_swaped(struct proc* p, uint64 );

// struct pglist* get_mru(struct proc* );

// struct pglist* get_free_ss(struct proc* );
// struct pglist* get_free_pg(struct proc* );
// // struct pglist* evict_mru(struct proc* p);

// struct pglist* find_ss(struct proc* , uint64 );//scan swapspace and try to find page there
// struct pglist* find_pg(struct proc* , uint64 );// try to find page first in freepg, then ss not found kill

// -------------------other helper function direct from the sysfile.c---------------------------
static struct inode* 
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }

  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;

  if(type == T_DIR){
    // now that success is guaranteed:
    dp->nlink++;  // for ".."
    iupdate(dp);
  }

  iunlockput(dp);

  return ip;

 fail:
  // something went wrong. de-allocate ip.
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

// int 
// copy_fdata(struct file *src, struct file *dst){
    
//     char buf[BSIZE];
//     int n;
//     uint off = 0;
//     ilock(src->ip);
//     ilock(dst->ip);


//     while(n = readi(src))


// }



//-----------------------------lovely functions----------------------------------
// called only when a process is allocating new pages
//mark the mapping of a va to pa in the freepg
//is ssbook = 1, you are free to mark va as 
int add_to_mru(struct proc* p, uint64 va, uint64 pa, int ssbook){
    // printf("addtomru::pid:: %d ::: %lu herre !\n", p->pid, pa);
    struct pglist* pg;
    pg = get_free_pg(p);
    if(!pg){
        printf("add_to_mru ::no free page\n");
        if(ssbook == 0){
            printf("your expectations are too high\n");
            return -1;
        }
    }
    else{
        pg->va = va;
        pg->pa = pa;
        pg->used = 1;
        
        return 0;
    }
    if(ssbook){


        pg = swap_out(p);

        
        if(pg->used){
            // printf("addtomru:mru page is not swapped out yet!!\n");
            return -1;
        }
        // freeing old page
        kfree((void *)pg->pa);
        pg->va = va;
        pg->pa = pa;
        pg->used = 1;
        
        
    }
    p->head = pg;
    // printf("hey i am using whatsapp!\n");
    return 0;
    
}
int remove_pa_from_mru(struct proc* p, uint64 pa){
    struct pglist* pg;
    for(pg = p->freepg; pg<&p->freepg[MAX_DPG]; pg++){
        if(pg->used == 0 && pg->pa == pa){
            break;
        }
    }
    if(pg - p->freepg >= MAX_DPG){
        printf("no free place!!!\n");
        return -1;
    }
    // kfree((void*)pg->pa);
    pg->pa = 0;
    pg->used = 0;
    return 0;

}

//--------------------------------------------------------------------

// Return the most recently used page that is still in memory
struct pglist* get_mru(struct proc* p) {
    struct pglist* start = p->head;
    struct pglist* pg = start;

    if (!pg) return 0;

    do {
        if (pg->used && (walk(p->pagetable, pg->va, 0) && (*walk(p->pagetable, pg->va, 0) & PTE_V))) {
            return pg;
        }
        pg++;
        if (pg >= &p->freepg[MAX_DPG])
            pg = p->freepg;  // wrap around
    } while (pg != start);

    return 0;  // no valid page found
}


struct pglist* get_free_pg(struct proc* p){
    struct pglist* pg;
    for(pg = p->freepg; pg<&p->freepg[MAX_DPG]; pg++){
        if(pg->used == 0){
            return pg;
        }
    }
    return 0;
}
struct pglist* get_free_ss(struct proc* p){
    for(int i = 0; i < SWAPM*MAX_DPG; i++)
        if(!p->swapspace[i].used){
            p->swapspace[i].used = 1;
            return &p->swapspace[i];
        }
    return 0;
}

//-----------------------ss and freepg-----------------------------------------
//----------------------------po-----------------------------------------------
struct pglist* find_ss(struct proc* p, uint64 va){
    uint64 vva = PGROUNDDOWN(va);
    struct pglist* pg;
    for(pg = p->swapspace; pg< &(p->swapspace[SWAPM*MAX_DPG]); pg++){
        if(pg->va == vva){
            return pg;
        }
    }
    return 0;
}

struct pglist* find_pg(struct proc* p, uint64 va){
    uint64 vva = PGROUNDDOWN(va);
    struct pglist* pg;
    for(pg = p->freepg; pg< &(p->freepg[MAX_DPG]); pg++){
        if(pg->va == vva){
            return pg;
        }
    }
    return 0;
}
//-----------------------------------------------------------------------------
//-------------------------reading writing functions --------------------------
//----------------------------alienated functions hehe -------------------------
// slot = idx in swapspace
// pa = for the page to write out
// called after getting a ss 
int write_swapfile(struct proc* p, int slot, uint64 pa){

    
    if(!p->swapfile){
        printf("no file name exist!\n");
        return -1;
    }
    struct file* f = p->swapfile;

    uint64 offset = slot*PGSIZE;

    begin_op();
    ilock(f->ip);
    writei(f->ip, 0, pa, offset, PGSIZE);
    //   f->off += r;
    iunlock(f->ip);
    end_op();
    
    return 0;

}

//read from file based on the slot registed in the ss
int read_swapfile(struct proc* p, int slot, uint64 pa){
    
    if(!p->swapfile){
        printf("no file name exist!\n");
        return -1;
    }
    struct file* f = p->swapfile;

    uint64 offset = slot*PGSIZE;

    begin_op();
    ilock(f->ip);
    readi(f->ip, 0, pa, offset, PGSIZE);
    //   f->off += r;
    iunlock(f->ip);
    end_op();
    
    return 0;

}

//- ---------------------------------------------------------------------

//-------------------------swapfile initiation and removal------------------
// create the swapfile with name pid.swap
void 
create_swapfile(struct proc *p) {
    
    struct inode *ip;
    char fname[16];

    safestrcpy(fname, "swap", sizeof(fname));
    fname[4] = '0' + (p->pid / 10);
    fname[5] = '0' + (p->pid % 10);
    fname[6] = '\0';
    
    begin_op();
    ip = create(fname, T_FILE, 0, 0);
    printf("noway you are doing this\n");
    if(ip == 0){
    panic("create_swapfile: create failed");
    }

    p->swapfile = filealloc(); 
    if(!p->swapfile)
        panic("cannot create swapfile");

    p->swapfile->type = FD_INODE;
    p->swapfile->ip = ip;
    p->swapfile->off = 0;
    p->swapfile->readable = 1;
    p->swapfile->writable = 1;

    iunlock(ip);
    end_op();
    return;
}

void remove_swapfile(struct proc *p) {
    if(p->swapfile) {
        fileclose(p->swapfile);
        p->swapfile = 0;
    }
    for(int i = 0; i < SWAPM*MAX_DPG; i++){
        p->swapspace[i].used = 0;
    }
}
//------------------------------------------------------------


//in swapspace get a empty entry 
// do not deallocate page
// data transfer from page to file
// mark *pte with PTE_S and ~PTE_V
struct pglist* swap_out(struct proc* p){ // va is already rouddown

    // printf("pid:: %d right from swap_out done!!\n", p->pid);
    //get mru
    if(p->swapfile == 0){
        create_swapfile(p);
    }
    struct pglist* vic = get_mru(p);

    if(!vic){
        printf("swapout :: stay \n");
        return 0;
    }

    vic->used = 0;

    uint64 vva = vic->va;

    // get the page pa
    pte_t *pte = walk(p->pagetable, vva, 0);

    uint64 pa = PTE2PA(*pte);

    // get free ss
    struct pglist* ss = get_free_ss(p);
    if(!ss){
        printf("no free space in ss!n");
        return 0;
    }

    
    if(write_swapfile(p, ss-p->swapspace, pa)!=0){
        printf("you fd while writing\n");
        return 0;
    }

    ss->va = vva;
    uint64 flags = PTE_FLAGS(*pte);

    *pte = (flags & ~PTE_V) | PTE_S;
    sfence_vma();

    printf("pid:: %d right from swap_out done!!\n", p->pid);
    return vic;

}

// call only after swap_oout, or when mru have already set to zero
// valid the pte too
//swap data to mru only
int swap_in(struct proc* p, uint64 va, struct pglist* pmru){

    printf("pid:: %d right from swap_indone!!\n", p->pid);
    // struct pglist* pg;

    struct pglist* ss = find_ss(p, va);
    if(!ss){
        printf("va page is not in swapspace!!!\n");
        return -1;
    }
    uint slot = ss - p->swapspace;



    // get mru
    // struct pglist* pmru = get_mru(p);
    if(pmru->used){
        printf("swapin::mru page is not swapped out yet!!\n");
        return -1;
    }

    pmru->va = va;

    read_swapfile(p, slot, pmru->pa);
    pmru ->used = 1;


    // seting again into work;
    pte_t *pte = walk(p->pagetable, va, 0);
    if(*pte & PTE_V){
        printf("are you ok why the hell you are swapping in the page already present\n");
        return -1;
    }
    if(!(*pte & PTE_S)){
        printf("are you ok why the hell you are swapping in page which is genuine invalid\n");
        return -1;
    }
    mappages(p->pagetable, va, PGSIZE,  pmru->pa, PTE_FLAGS(*pte));


    *pte = (*pte & ~PTE_S);

    sfence_vma();

    printf("pid:: %d right from swap_indone!!\n", p->pid);

    return 0;

}



