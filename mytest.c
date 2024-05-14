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
    //uintmmap(uintaddr, int lenth, int prot, int flags, int fd, int offset)
    
    printf(1,"Frist Page : %d!\n",freemem());
    int fd = open("README",O_RDWR);
    if(fd == -1) exit();
    char* temp = (char*)mmap(0,4096,PROT_READ|PROT_WRITE,MAP_POPULATE,fd,0);
    char s0 = temp[0],s10 = temp[10];
    temp[0] = '1',temp[10] = 0;
    printf(1,"%s\n",temp);
    temp[0] = s0,temp[10] = s10;
    printf(1,"Free Page : %d!\n",freemem());

    char* temp2 = (char*)mmap(4096,4096,PROT_READ|PROT_WRITE,MAP_ANONYMOUS,-1,0);
    if(temp2 == 0) printf(1,"FAILED SOMETHING....\n"),exit();
    printf(1,"Free Page : %d!\n",freemem());
    
    int fd2 = open("README",O_RDWR);
    char* temp3 = (char*)mmap(8192,4096,PROT_READ|PROT_WRITE,0,fd2,1); //this will occur pagefault.
    if(temp3 == 0) exit();
    printf(1,"%c\n",temp3[0]);
    printf(1,"%c\n",temp3[0]);
    printf(1,"%c\n",temp3[0]);
    char s0 = temp3[0],s10 = temp3[10];
    temp3[0] = '1',temp3[10] = 0;
    printf(1,"%s\n",temp3);
    temp3[0] = s0,temp3[10] = s10;
    printf(1,"Free Page : %d!\n",freemem());

    char* temp4 = (char*)mmap(12288,4096,PROT_READ|PROT_WRITE,MAP_POPULATE|MAP_ANONYMOUS,-1,0); //this will occur pagefault.
    if(temp4 == 0) exit();
    printf(1,"Free Page : %d!\n",freemem());
    exit();
}
