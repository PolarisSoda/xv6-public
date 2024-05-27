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
#include "x86.h"
#include "proc.h"
#include "elf.h"

int main () {
    for(int i=0; i<PHYSTOP/4096; i++) {
    }
	int a, b;

    swapstat(&a, &b);
    exit();
}
