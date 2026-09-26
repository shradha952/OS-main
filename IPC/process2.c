#include "kernel/types.h"
#include "user/user.h"



int main(){

    int shm_key = 5, pos = 2, mb_key, *shared;

    uint64 va = get_shm(shm_key);

    accesss(shm_key);
        shared = (int *)va;
        mb_key = *shared;
    signals(shm_key);

    int x, y, z;
    while(1){
        

        y = recv_msg(mb_key);
        accesss(shm_key);
            x = shared[pos];
            z = shared[y];
        signals(shm_key);

        send_msg(mb_key, x);

        
        if( z == y){
            break;
        }
        else{
            pos = y;
        }
        


    }
    
    shm_close(shm_key);
    exit(0);

}