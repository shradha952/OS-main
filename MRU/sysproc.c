#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
 

  acquire(&tickslock);
  
  struct proc *p = myproc();
  acquire(&p->lock);
  p->rtime -= n;
  release(&p->lock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}



// -------------------lest ask for clarification--------------------------------

// this is me here creating some top shit

uint64 //--------------------changed2
sys_top(void){

    extern void procdump();
    printf("\npid\tstate\tname\trtime");
    procdump();
    return 0; 

}

uint64
sys_setpriority(void){
  int n;
  argint(0, &n);

  return set_priority(n);

}

// ---------------SHM and mailbox system calls------------------------------------------
uint64
sys_shm_create(void){
    int va, key;
    argint(0, &key);
    printf("creating shared memory!!!");

    if((va = create_shm(key)) == 0){
      printf("hart %d: shm for this key %d already exist", cpuid(), key);
      return 0;
    }
    return va;

}

uint64
sys_accesss(void){
  int k;

  argint(0, &k);

  return accesss(k);
}

uint64
sys_signals(void){
  int k;
  argint(0, &k);
  return signals(k);
}


uint64
sys_get_shm(void){
  int k;
  argint(0, &k);

  uint64 va = get_shm(k);
  return va;
}

uint64 
sys_shm_close(void){
  int k;
  argint(0, &k);

  return close_shm(k);
}


uint64
sys_send_msg(void){
  int k, val;
  argint(0, &k);
  argint(1, &val);

  return msg_send(k, val);
}

uint64
sys_recv_msg(void){
  int k;
  argint(0, &k);
  return msg_recv(k);
}


// ------------------memory mangement related incomplete---------------------------

uint64
sys_check_accessed(void)
{
    uint64 va;
struct proc *p = myproc();
pte_t *pte;

argaddr(0, &va);
pte = walk(p->pagetable, va, 0);
if(!pte)
    return -1;

// return 1 if PTE_A set, 0 otherwise
return ((*pte & PTE_A) != 0);

}
// uint64
// sys_getpagestat(void){
//     int pid; uint64 addr;
//     argint(0, &pid);
//     argaddr(1, &addr);
// }