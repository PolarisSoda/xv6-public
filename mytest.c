#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    ps(0);
    for(int i=1; i<=10; i++) {
        if(setnice(i,i) == -1) printf(1,"Failed!\n");
    }
    for(int i=1; i<=10; i++) {
        int ret = getnice(i);
        if(ret != -1) printf(1,"%d's nice value is %d\n",i,ret);
    }
    ps(0);
    exit();
}