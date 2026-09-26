
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "mbox.h"


struct mailbox mailboxes[MX_MAILBOXES];

// write_head - will write at this place , read_head- will read from this place
void mailbox_init(void){

    for(int i = 0; i<MX_MAILBOXES; i++){
        initlock(&(mailboxes[i].lk), "mailbox");
        mailboxes[i].available = 1;
        // sender == available mean same sender
    }

}

int msg_send(int key, int val){

    if(key < 0 || key > MX_MAILBOXES){
        printf("out of bound referencing!!!");
        return 2;
    }
    
    struct mailbox *mb = &mailboxes[key];
    //acquire lock
    struct proc *p = myproc();
    acquire(&(mb->lk));

    if(mb->available == 0){
        procdump();
        sleep(mb, &(mb->lk));
        
        // return -1;//for the previous msg is not read yet
    };
    mb->msg.val = val;
    mb->available = 0;
    // mb->sender = !mb->sender;
    printf("CPU: %d :: pid = %d, name = %s, sent_data = %d \n", cpuid(), p->pid, p->name, val);
    wakeup(mb);
    sleep(mb, &(mb->lk));
    release(&(mb->lk));
    return 0; // successfully written
}

int msg_recv(int key){
    int out;
    if(key < 0 || key > MX_MAILBOXES){
        printf("out of bound referencing!!!");
        return 0;
    }
    struct mailbox *mb = &mailboxes[key];
    struct proc *p = myproc();
    acquire(&(mb->lk));
        if(mb->available == 1){
             procdump();
            sleep(mb, &(mb->lk));       
             // any new msg is not written yet;
        }
        
        out = mb->msg.val;
        printf("CPU: %d :: pid = %d, name = %s, recv_data = %d \n", cpuid(), p->pid, p->name, out);
        mb->available =1;
        wakeup(mb);
        // sleep(mb, &(mb->lk));
    release(&(mb->lk));
    return out; // no error

}


