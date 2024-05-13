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
    printf(1,"Free Page : %d!\n",freemem());
    int fd = open("README",O_RDWR);
    char* temp = (char*)mmap(0,4096,PROT_READ|PROT_WRITE,MAP_POPULATE,fd,0);
    printf(1,"%s\n",temp);
    //printf(1,"%d\n",freemem());
    exit();
    
}
