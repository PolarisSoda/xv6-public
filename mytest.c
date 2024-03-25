#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    for(int i=1; i<=20; i++) {
        if(setnice(i,12) == -1) printf(1,"Failed at %d\n",i);
    }
    for(int i=1; i<=20; i++) {
        int ret = getnice(i);
        if(ret == -1) printf(1,"FAILED at %d\n",i);
        else printf(1,"SUCCESS ON %d\n",ret);
    }
    ps(0);
    ps(1);
    exit();
}