// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

struct spinlock pages_lock;
int use_pages_lock;
struct page pages[PHYSTOP/PGSIZE] = {0,}; //이건 그냥 page를 관리하는 관리체.
struct page *page_lru_head; //이게 LRU PAGE들을 관리하는 Circulat LIST.
int num_free_pages = PHYSTOP/PGSIZE;
int num_lru_pages = 0;
char swap_bit[SWAPMAX/64];
// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  initlock(&pages_lock, "pages_lock");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
  use_pages_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(char *v) {
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

int reclaim() {
  if(use_pages_lock) acquire(&pages_lock);
  struct page *cur = page_lru_head;
  for(int i=0; i<num_lru_pages; i++) {
    pte_t* now_pte = walkpgdir(cur->pgdir,cur->vaddr,0);
    if(!now_pte) panic("ERROR OCCURED"); //없을수가 있나?

    if(*now_pte&PTE_A) {
      *now_pte &= ~PTE_A; //clear PTE_A;
    } else {
      char* phy_addr = (char*)P2V(PTE_ADDR(*now_pte));
      for(int i=0; i<SWAPMAX/8; i++) {
        if(!swap_bit[i]) {
          swap_bit[i] = 1;
          swapwrite(phy_addr,i);
          *now_pte &= ~PTE_P;
          break;
        }
      }
      kfree(phy_addr);
    }
    cur = cur->next;
  }
  //SUCCESS:
  if(use_pages_lock) release(&pages_lock);
  return 1;
  
  //FAIL:
  if(use_pages_lock) release(&pages_lock);
  return 0;
}
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char* kalloc(void) {
  struct run *r;

//try_again:
  if(kmem.use_lock)
    acquire(&kmem.lock);
  RETRY:
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
  } else {
    //there's no physical memory. so we have to swap it.
    if(!reclaim()) panic("OOM!!!!!!!");
    goto RETRY;
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

