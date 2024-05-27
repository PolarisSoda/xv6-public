#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
extern struct page pages[PHYSTOP/PGSIZE];
extern struct page *page_lru_head;
extern int num_lru_pages;
extern struct spinlock pages_lock;
extern int use_pages_lock;
extern char swap_bit[SWAPMAX/64+1];
extern char nothing[4096];
extern struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    uint pft_addr = PGROUNDDOWN(rcr2());
    pte_t *pte = walkpgdir(myproc()->pgdir,(void*)pft_addr,0);
    uint offset = PTE_ADDR(*pte) >> PTXSHIFT;
    uint perm = PTE_FLAGS(*pte);
    if(kmem.use_lock && holding(&kmem.lock)) release(&kmem.lock);
    char* mem = kalloc();
    if(kmem.use_lock && !holding(&kmem.lock)) acquire(&kmem.lock);
    if(mem == 0) panic("what?");
    if(offset-- == 0 && offset <= 1555 && swap_bit[offset] != 0) {
      swapwrite(mem,offset<<3);
      swapread(nothing,offset<<3);
      swap_bit[offset] = 0;
    } else {
      memmove(mem,(char*)P2V(mem),PGSIZE);
    }
    *pte = V2P(mem) | perm | PTE_P;
    struct page *cur = &pages[V2P(mem)/PGSIZE];
    if(*pte&PTE_U) {
      if(!page_lru_head) {
        page_lru_head = cur;
        page_lru_head->next = cur, page_lru_head->prev = cur;
      } else {
        cur->next = page_lru_head;
        cur->prev = page_lru_head->prev;
        page_lru_head->prev = cur;
        page_lru_head = cur;
      }
      num_lru_pages++;
    }
    /*
    *pte = V2P(new_space) | perm | PTE_P;
    swap_bit[offset] = 0;
    int idx = V2P(new_space)/PGSIZE;
    if(idx < PHYSTOP/PGSIZE) {
      struct page *cur = &pages[idx];
      cur->pgdir = myproc()->pgdir;
      cur->vaddr = (char*)pft_addr;
      if(*pte&PTE_U) {
        if(!page_lru_head) {
          page_lru_head = cur;
          page_lru_head->next = cur, page_lru_head->prev = cur;
        } else {
          cur->next = page_lru_head;
          cur->prev = page_lru_head->prev;
          page_lru_head->prev = cur;
          page_lru_head = cur;
        }
        num_lru_pages++;
      }
      
    }
    */
    //accessing swapped page will occur page fault.
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
