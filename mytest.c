#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    for(int i=1; i<=20; i++) {
        if(setnice(i,12) == -1) printf(1,"Failed at %d\n",i);
    }
    ps(0);
    ps(1);
    exit();
}