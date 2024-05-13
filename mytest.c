#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "proc.h"
#include "syscall.h"

//1억번 = 1초였던가.
//약 15초동안 cpu를 혹사시킵니다.
void starvation() {
    for(int i=0; i<1500000000; i++) asm("");
}

int main() {
    int fd = open("README",O_RDWR);
    uint temp = mmap(0,4096,PROT_READ|PROT_WRITE,MAP_POPULATE,fd,0);
    
    cprintf("%c",&temp);
    exit();
}
