// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
// ----------------------------------------
#define PGRFSIZE ((PHYSTOP - KERNBASE) / PGSIZE)
// ----------------------------------------
// struct pgm{
//     struct spinlock lock;
//     uint rfcnt;
// };

uint pgalloc[PGRFSIZE];
struct spinlock pglk;
static uint pa2key(uint64 pa){
  return (pa - KERNBASE) / PGSIZE;
}

// -------------------------------------




void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  pgalloc_init();
  freerange(end, (void*)PHYSTOP);

  
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 key = pa2key((uint64)pa);

  // printf("hello key: %lu\n", PGRFSIZE);
  acquire(&kmem.lock);
  acquire(&pglk);

  if(pgalloc[key] > 1){
    pgalloc[key]--;
    release(&pglk);
    release(&kmem.lock);
    return;
  }

  if(pgalloc[key] == 0){
    printf("%lu hello babe from kfree\n", (uint64)pa);
    panic("kfree: refcount 0");}

  memset(pa, 1, PGSIZE);
  pgalloc[key] = 0;

  r = (struct run*)pa;
  r->next = kmem.freelist;
  kmem.freelist = r;

  release(&pglk);
  release(&kmem.lock);
  // printf("kfree ::: %lu\n", (uint64)pa);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  // uint key = (uint64)pa / PGSIZE;
  // printf("when i am busy do not disturb!!!\n");

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  uint64 key = pa2key((uint64)r);
  pgalloc[key] = 1;
  
  // printf("kalloc.c ::: here %lu\n", (uint64)r);
  return (void*)r;
}


// ---------------------------------------------


void 
pgalloc_init(){
  initlock(&pglk, "pgrflk");
  for(int i = 0; i<PGRFSIZE; i++){
    pgalloc[i] = 1;
  }
  return;
}
void
dec_pgrf(uint64 pa){
  uint64 key = pa2key(pa);
  // struct pgm *pg = &pgalloc[key];
  acquire(&pglk);
  --pgalloc[key];
  release(&pglk);
  return;
} 
void
inc_pgrf(uint64 pa){
  uint64 key = pa2key(pa);
  // struct pgm *pg = &pgalloc[key];
   acquire(&pglk);
  ++pgalloc[key];
  release(&pglk);
  return;
} 

uint
get_pgrf(uint64 pa){
  uint64 key = pa2key(pa);
  // struct pgm *pg = &pgalloc[key];
  uint out;
  acquire(&pglk);
  out = pgalloc[key];
  release(&pglk);
  return out;
}
