#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
    int pid = getpid();
    int child, a = 1;
    ps(0);
    setnice(pid,1);
    ps(0);
    child = fork();
    if(child == 0) {
        for(int i=1; i<=100000; i++) for(int j=1; j<=100000; j++) asm(""), a += i+j;
        printf(1,"childsjlfkjs\n");
        ps(0);
        exit(); 
    } else {
        setnice(pid,20);
        for(int i=1; i<=100000; i++) for(int j=1; j<=100000; j++) asm(""), a += i+j;
        printf(1,"pt\n");
        ps(0);
        wait();
    }
    printf(1,"WRWERWERWERWERWERWERWERW\n");
    ps(0);
    exit();
}
