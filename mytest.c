#include "types.h"
#include "stat.h"
#include "user.h"

//1억번 = 1초였던가.
//약 15초동안 cpu를 혹사시킵니다.
void starvation() {
    for(int i=0; i<1'500'000'000; i++) asm("");
}

int main() {
    int pid = fork();
    if(pid == 0) {
        //child
        int ppid = fork();
        if(ppid == 0) {
            starvation();
            printf(1,"C\n");
            ps(0);
            exit();
        } else {
            setnice(ppid,17);
            starvation();
            printf(1,"P\n");
            ps(0);
            wait();
            exit();
        }
    } else {
        //grand-parent
        setnice(pid,10);
        starvation();
        printf(1,"GP\n");
        ps(0);
        wait();
    }
    ps(0);
    exit();
}
