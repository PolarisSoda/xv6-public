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


int main() {
    int fd = open("README",O_RDWR);
    printf(1,"%d!",freemem());
    uint temp = mmap(0,8192,PROT_READ|PROT_WRITE,MAP_POPULATE,fd,4096);
    write(1,(void*)temp,12);
    printf(1,"%d\n",freemem());
    exit();
}
