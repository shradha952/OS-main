#include "kernel/types.h"
#include "user/user.h"


void insert(int *arr, int i, int val){
    arr[i] = val;
    return;
};


int main(){

    int shm_key = 5;
    int mb_key = 7;
    int pos = 1;
    int *shared;
    uint64 va = shm_create(shm_key);

    accesss(shm_key);

        if(fork() == 0){

            printf("MASTER2: creating process2\n");
            char *args[] = {"process2\0", 0};

            exec("./process2", args);
            exit(0);
        }

        
        shared = (int *)va;
        *shared = mb_key;

        insert(shared, 1, 3);
        insert(shared, 2, 5);
        insert(shared, 3, 4);
        insert(shared, 4, 7);
        insert(shared, 5, 8);
        insert(shared, 6, 10);
        insert(shared, 7, 10);
        insert(shared, 8, 9);
        insert(shared, 9, 6);
        insert(shared, 10, 10);



    signals(shm_key);
    int x, y;

    while(1){

        accesss(shm_key);
        x = shared[pos];
        signals(shm_key);

        send_msg(mb_key, x);

        y = recv_msg(mb_key);

        if(pos == y){
            break;
        }
        else{
            pos =y;
        }

    }

    wait(0);

    printf("A's location :: %d, B's location :: %d\n", x, y);
    shm_close(shm_key);

    exit(0);

}
