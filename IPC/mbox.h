

#define MX_MAILBOXES 32

struct msg{ // message size single int
    int val;
};
struct mailbox{ // 
    struct spinlock lk;
    struct msg msg;
    int available; // 1-> available for writing, 0->available fo rreading
    
};

extern struct mailbox mailboxes[MX_MAILBOXES];