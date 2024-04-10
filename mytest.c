#include "types.h"
#include "user.h"
#include "stat.h"

int main() {
    int a = 1;
    
    int pid = fork();
    if(pid != 0) setnice(pid,0);
    sleep(1);
    ps(0);
    sleep(10);
    ps(0);
    if(pid != 0) wait();
    exit();
}