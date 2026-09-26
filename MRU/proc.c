#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];
// struct listnode prochead;
struct proc *initproc;

//----------------------------------------changed

int nextpid = 2;//for primes, start with nextpid = 2, before it
// was just auto increment pid number concept with nextpid = 1;

// dynamic array to store mark primes prime ==0, currently in use ==2 else avail
#define PRIME_CAP 1024
int primes[PRIME_CAP];

static void fill_prime(){
    for(int i = 2; i<PRIME_CAP; i++){
      if(primes[i] == 0)
      for(int j = i*i; j<PRIME_CAP; j+=i){
        primes[j] = 1;}}
    return;
}

static void init_primes(){    
    fill_prime();
    return;
}

static int next_pid(){
    int npid = -1;
    for(int i = 2; i<PRIME_CAP; i++){
      if(primes[i] == 0){npid = i;break;}}
    if(npid == -1){
      printf("next_pid : no new pid available!!");
      exit(1);
    }
    return npid;
}

static void set_prime(int pid){
    primes[pid] = -1;
    return;
}
static void free_prime(int pid){
    primes[pid] = 0;
    return;
}

//-------------------------------------------------

struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
      //------------------------------------------
      // for(int i = 0 ; i<1024; i++){
      //   p->is_swapped[i] = 0;
      // }
      // p->frtime = 0;
  }

  // -------------------
  mailbox_init();
  shm_init();
  init_primes();//-------------------cahnged

}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}





// helper fuction to find to get nextpid
int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = next_pid();
  set_prime(pid);

  // printf("---------------------------------------\n");//----------------changed
  // printf("my new prime pid: %d\n ", pid);
  // printf("---------------------------------------\n");
  release(&pid_lock);

  return pid;
}


//

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->last_sch = 0;
  p->rtime = 0;
  p->state = USED;
  p->priority = DEFPRIORITY;
  p->timeslice = 0;
  p->counter = 0;
  p->sleep_time = 0;
  //-----------------------
  p->page_fault = 0;
  p->swap_in = 0;
  p->swap_out = 0;

  for(int i = 0; i<MAX_DPG; i++){
    p->freepg[i].va = 0;
    p->freepg[i].pa = 0;
    p->freepg[i].used = 0;

  }
  for(int i = 0; i<SWAPM*MAX_DPG; i++){
    p->swapspace[i].va = 0;
    p->swapspace[i].pa = 0;
    p->swapspace[i].used = 0;

  }
  p->head = &p->freepg[MAX_DPG-1];
  p->swapfile = 0;
  // printf("hello lady straight from userint\n");
  // create_swapfile(p);
  
  
  //---------intializing key2va
  for(int i = 0; i<512; i++){
    p->key2va[i] = -1;
  }

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;


  free_prime(p->pid); //----------------------
  // printf("---------------------------------------\n");//--------------changed
  // printf("pid freed : %d\n", p->pid);
  // printf("---------------------------------------\n");
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->priority = 0;
  p->timeslice = 0;
  p->counter = 0;
  p->rtime = 0;
  p->state = UNUSED;
  remove_swapfile(p);
  p->head = 0;

  for(int i = 0; i<MAX_DPG; i++){
    p->freepg[i].va = 0;
    p->freepg[i].pa = 0;
    p->freepg[i].used = 0;

  }
  for(int i = 0; i<SWAPM*MAX_DPG; i++){
    p->swapspace[i].va = 0;
    p->swapspace[i].pa = 0;
    p->swapspace[i].used = 0;

  }
  
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{ 
  // struct proc *p = myproc();
  // printf("name : %s, pid : %d, hello from freeproctable\n", p->name, p->pid);
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  // printf("baby after uvmunmap\n");
  printf("hello\n");
  uvmfree(pagetable, sz);
  
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  
  p = allocproc();
  
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
   
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
  // -------------------------changed
  p->personal_space = 1;
  // -------------------------------
  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{ 
  
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  // printf("i was here %d\n", p->pid);
  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  p->personal_space = 0;
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    // printf("hello baby\n");
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  // swap spacce copy
  if(p->swapfile){
  char bffr[BSIZE];
  for(int i = 0; i<SWAPM*MAX_DPG; i++){
    if(p->swapspace[i].used){

      // --------------data copying from one file to other
      uint bfoff = i*PGSIZE;
      uint bfx = bfoff + PGSIZE;
      int bfn = 0;
      
      

      ilock(p->swapfile->ip);
      ilock(np->swapfile->ip);

      begin_op();

      while(bfoff < bfx){
        if((bfn = readi(p->swapfile->ip, 0, (uint64)bffr,bfoff, BSIZE)) > 0){
          writei(np->swapfile->ip, 0, (uint64)bffr, bfoff, BSIZE);
          bfoff+= bfn;
        }
      }

      iunlock(np->swapfile->ip);
      iunlock(p->swapfile->ip);
      end_op();

      // ---------------------------------------------------

    np->swapspace[i].va = p->swapspace[i].va;
    np->swapspace[i].pa = p->swapspace[i].pa;
    np->swapspace[i].used = p->swapspace[i].used;

    }
  }
}
  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  // printf("fork ::: exit forked ::: %s\n", np->name);
  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);
  p->rtime++;
  // printf("Total running time: %lu, pid= %d\n", p->rtime, p->pid);
  printf("cpu: %d\tExiting %s (pid=%d, burst_time=%u)\n",cpuid(), p->name, p->pid, p->timeslice);
  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.

void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    intr_on();
    intr_off();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        uint start_ticks = ticks;
        p->state = RUNNING;
      
        // printf("hello: %u\n", ticks);
         //----------------changed2
        c->proc = p;
        swtch(&c->context, &p->context);

        p->rtime += (ticks - start_ticks + 1);
        // printf("cup: %d")
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      asm volatile("wfi");
    }
  }
 }
// -------------------------------------------------------------------------------changed
// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();
//   c->proc = 0;

//   for(;;){
//     intr_on();

//     struct proc *pp = 0;
//     int min_time = 2000;
   
//     for(p = proc; p < &proc[NPROC]; p++){
//       acquire(&p->lock);
//       if(p->state == RUNNABLE){
//         if(p->timeslice < min_time){
//           if(pp) release(&pp->lock);
//           pp = p;
//           min_time = p->timeslice;
//         } else {
//           release(&p->lock);
//         }
//       } else {
//         release(&p->lock);
//       }
//     }

//     if(pp){
//       pp->state = RUNNING;
//       c->proc = pp;

//       uint start_time = ticks;
//       swtch(&c->context, &pp->context);
//       pp->rtime = (ticks - start_time + 1);

//       c->proc = 0;
//       release(&pp->lock);
//     } else {
//       asm volatile("wfi");
//     }
//   }
// }

// -----------------------------------------------------
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();
  // printf("lovely2: %u\n", ticks);
  //  p->rtime += ticks - p->last_sch;

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  
  acquire(&p->lock);
  p->state = RUNNABLE;
  p->counter = p->timeslice;
  sched();

  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Sleep on wait channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  p->sleep_time++;
  

  // --(p->rtime);
  // printf("here from sleep: %u, %lu, %lu\n", ticks, p->last_sch, (ticks - p->last_sch));
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on wait channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  
  
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d\t%s\t%s\t%lu", p->pid, state, p->name, p->rtime);
    printf("\n");
  }
  
}




// ----------------ask for any clarification-----------------------------------------------------

uint64 set_priority(int n){


    struct proc *p = myproc();
    // if(p == 0){return -1;}
    acquire(&p->lock);
    
    p->priority =n; 
    p->timeslice = assign_tick(n);
    p->counter = p->timeslice;
    // printf("my baby from set priority: %u, %u\n", p->timeslice, p->pid);
    release(&p->lock);
    return 0;
}

uint assign_tick(int n){
    uint x = 1;
    while(x*x <= n)x++;
    return (x-1);
}






// struct proc*
// find_proc(int pid){
//   struct proc* p;
//   for(p = proc; p < proc[NPROC]; p++){
//     acquire(&p->lock);
//     if(p->pid == pid){
//       release(&p->lock);
//       break;
//     }
//     release(&p->lock);
//   }

//   return p;
// }

// int getpagestat(int pid, struct pagestat *st){
//     struct proc* p = find_proc(pid);
//     struct pagestat st2 = {p->page_fault, p->swap_in, p->swap_out};

//     copyout(p->pagetable, (uint64)st, (char *)st, sizeof(st));
    
// }