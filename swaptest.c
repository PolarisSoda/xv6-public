#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "mmu.h"

extern struct page pages[PHYSTOP/PGSIZE];
extern struct page *page_lru_head;
int main () {
    for(int i=0; i<PHYSTOP/4096; i++) {
        printf(1,"%d %d\n",i,(int)pages[i].vaddr);
    }
	int a, b;

    swapstat(&a, &b);
}
