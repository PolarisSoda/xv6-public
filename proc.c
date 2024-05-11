#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;
struct mmap_area mmap_area_array[64];
static struct proc *initproc;

int nextpid = 1;
int weight[40] = {
  88761, 71755, 56483, 46273, 36291,
  29154, 23254, 18705, 14949, 11916,
  9548 , 7620 , 6100 , 4904 , 3906 ,
  3121 , 2501 , 1991 , 1586 , 1277 ,
  1024 , 820  , 655  , 526  , 423  ,
  335  , 272  , 215  , 172  , 137  ,
  110  , 87   , 70   , 56   , 45   ,
  36   , 29   , 23   , 18   , 15   ,
}; //weight actually define[PROJ2]

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
//allocproc was modified to init nice value[PROJ1]
//allocproc was modified to init runtimes and time_slice[PROJ2]
static struct proc* allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->nice = 20; //priority values[PROJ1]
  p->v_runtime = 0; //initializing runtime values[PROJ2]
  p->r_runtime = 0;
  p->t_runtime = 0;
  p->time_slice = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
// fork was modified to inherit the runtime and values[PROJ2]
int fork(void) {
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  // TODO: t_runtime,v_runtime,nice inherit
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->nice = curproc->nice;
  np->v_runtime = curproc->v_runtime;
  np->t_runtime = curproc->t_runtime;
  np->r_runtime = 0; 

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    struct proc *minp = 0;
    int min_vrt = 0x7FFFFFFF, t_weight = 0;
    
    acquire(&ptable.lock);
    for(p = ptable.proc; p<&ptable.proc[NPROC]; p++) {
      if(p->state != RUNNABLE) continue;
      t_weight += weight[p->nice];
      if(min_vrt > p->v_runtime) min_vrt = p->v_runtime, minp = p;
    }
    release(&ptable.lock);
    
    if(minp) {
      minp->time_slice = 10000*weight[minp->nice]/t_weight;
      minp->r_runtime = 0;
      c->proc = minp;
      switchuvm(minp);
      minp->state = RUNNING;
      swtch(&(c->scheduler), minp->context);
      switchkvm();
      c->proc = 0;
    }
    
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
// How about woken process?
// we have to change some v_runtime.
// if there's no process in RUNNABLE. v_runtime is 0 else calculate with formula.[PROJ2]
static void wakeup1(void *chan) {
  struct proc *p;
  int min_vrt = 0x7FFFFFFF;
  int run_exist = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state != RUNNABLE) continue;
    if(min_vrt > p->v_runtime) min_vrt = p->v_runtime, run_exist = 1;
  }
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
      if(run_exist) p->v_runtime = min_vrt - (1000<<10)/weight[p->nice]; 
      else p->v_runtime = 0;
    }
  }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

//get nice value of certain process.
int getnice(int pid) {
  int ret = -1;
  struct proc *p;

  acquire(&ptable.lock);
  for(p=ptable.proc; p<&ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      ret = p->nice;
      release(&ptable.lock);
      return ret;
    }
  }
  release(&ptable.lock);
  return ret;
}

//set nice value of certain process
int setnice(int pid,int n_val) {
  if(n_val < 0 || n_val >= 40) return -1; //Invalid nice value

  struct proc *p;

  acquire(&ptable.lock);
  for(p=ptable.proc; p<&ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      p->nice = n_val;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void ps(int pid) {
  struct proc *p;
  int first = 0;
  const char *str[] = {"UNUSED","EMBRYO","SLEEPING","RUNNABLE","RUNNING","ZOMBIE "};

  acquire(&ptable.lock);
  for(p=ptable.proc; p<&ptable.proc[NPROC]; p++) {
    if((pid == 0 || p->pid == pid) && p->state > 1) {
      //Except UNUSED and EMBRYO
      if(first == 0) {
        cprintf("name\t pid\t state\t\t priority\t");
        cprintf("runtime/weight\t runtime\t v_runtime\t tick %d\n",ticks*1000);
        first = 1;
      }
      cprintf("%s\t %d\t %s\t %d\t\t",p->name,p->pid,str[p->state],p->nice);
      cprintf("%d\t\t %d\t\t %d\n",p->t_runtime/weight[p->nice],p->t_runtime,p->v_runtime);
    }
  }
  release(&ptable.lock);
}

uint mmap(uint addr,int length,int prot,int flags,int fd,int offset) {
  //important!!!!!! when we get mmap_area, then please reset addr to zero when error occured.
  //we determine whether this area is using by checking address is bigger than MMAPBASE;

  /* Define Some Variables */
  struct proc *p = myproc();
  struct mmap_area *mmap_cur; //pointer that will indicate be used mmap_area
  struct file *f = fd == -1 ? 0 : filedup(p->ofile[fd]);
  uint sam = addr + MMAPBASE; //start address of memory
  int p_cnt = length/PGSIZE; //count of page
  int PW = (flags&PROT_WRITE); //is it writable?
  char *tmp_memory[1<<10]; //when mappng pages and alloc, there's will error. then we have to empty mappage and physical memory.
  int t_cnt = 0; //tmp_memory index

  /* Determining whether condition is apporpriate*/
  if(addr%PGSIZE || length%PGSIZE) return 0; //addr is always page-aligned, length is also a multiple of page size
  if((flags&MAP_ANONYMOUS)!=MAP_ANONYMOUS && fd == -1) return 0; //It's not anonymous, but when the fd is -1.
  if(f && ((prot&PROT_READ)^(f->readable))) return 0; //when have file, check read permission same
  if(f && ((prot&PROT_WRITE)^(f->writable))) return 0; //when have file, check write permission same
  if(!f && offset != 0) return 0; //offset is given for only fd. if fd is not given, it should be 0.

  /* Real Code that Execute mmap*/
  for(int i=0; i<64; i++) if(mmap_area_array[i].addr < MMAPBASE) {mmap_cur = &mmap_area_array[i]; goto found;}
  return 0; //there's no empty space in mmap_area_array

  found:
  mmap_cur->f = fd == -1 ? 0 : p->ofile[fd];
  mmap_cur->addr = addr;
  mmap_cur->length = length;
  mmap_cur->offset = offset;
  mmap_cur->prot = prot;
  mmap_cur->flags = flags;
  mmap_cur->p = p;

  // we have to divide several cases.
  if(flags <= 1) {
    //no page table allocation needed. just return start address.
    //it seems that kalloc is not needed.
    return sam;
  } else if(flags == 2) {
    //MAP_POPULATE
    //PAGE TABLE 만들어야 함.
    //실제 PHY MAPPING
    //not anonymous, so it have a file.
    //kalloc and fill with actual file's context.
    uint t_off = f->off;
    f->off = offset;
    for(t_cnt=0; t_cnt<p_cnt; t_cnt++) {
      char *phy_addr = tmp_memory[t_cnt] = kalloc(); //tmp_memory[t_cnt]를 언제 다쓰고 있니?
      if(phy_addr == 0) goto DIE_IN; //physical address를 구할수 없었습니다. //이전거 다 밀어야 함.
      memset(phy_addr,0,PGSIZE); //생각해보니 항상 다읽어온다는 보장이 없으니 싹싹밀게요. 원하지 않는 게 나올 수도 있어서.
      if(fileread(f,phy_addr,PGSIZE) == -1) goto EL_FAIL; //fileread should be check. //이미 할당되서 이번걸 밀어야 함.
      if(mappages(p->pgdir,(void*)(sam + PGSIZE*t_cnt),PGSIZE,V2P(phy_addr),PW|PTE_U) == -1) goto EL_FAIL; //이미 할당되서 이번걸 밀어야 함. //이건 나중에 생각하자.
    }
    f->off = t_off;
    return sam;
  } else if(flags == 3) {
    //MAP_ANONYMOUS | MAP_POPULATE
    for(t_cnt=0; t_cnt<p_cnt; t_cnt++) {
      char *phy_addr = tmp_memory[t_cnt] = kalloc();
      if(phy_addr == 0) goto DIE_IN;
      memset(phy_addr,0,PGSIZE);
      if(mappages(p->pgdir,(void*)(sam + PGSIZE*t_cnt),PGSIZE,V2P(phy_addr),PW|PTE_U) == -1) goto EL_FAIL;
    }
    return sam;
  } else {mmap_cur->addr = 0; return 0;} //Not Defined Flag -> FAIL;

  EL_FAIL:
  //t_cnt가 할당은 됨.
  memset(tmp_memory[t_cnt],0,PGSIZE);
  kfree(tmp_memory[t_cnt]);
  DIE_IN:
  //t_cnt-1 까지 kfree 및 page 삭제
  for(int i=0; i<t_cnt; i++) {
    pte_t *PTE;
    //walkpgdir(pde_t *pgdir, const void *va, int alloc)
    PTE = walkpgdir(p->pgdir,(void*)(mmap_cur->addr+i*PGSIZE),0);
    *PTE = 0;
    memset(tmp_memory[t_cnt],0,PGSIZE);
    kfree(tmp_memory[t_cnt]);
  }
  mmap_cur->addr = 0;
  return 0;
}

int mummap(uint addr) {
  struct proc *p = myproc(); //now process
  struct mmap_area *mmap_cur = 0;

  for(int i=0; i<64; i++) if(mmap_area_array[i].addr == addr) {mmap_cur = &mmap_area_array[i]; goto found;}
  return -1;

  found:
  int p_cnt = mmap_cur->length/PGSIZE;
  for(int i=0; i<p_cnt; i++) {
    pte_t *PTE;
    PTE = walkpgdir(p->pgdir,(void*)(mmap_cur->addr+i*PGSIZE),0); //page tabe entry가 나온다.
    if(PTE != 0 && (*PTE&PTE_P)) {
      char* VA = P2V(PTE_ADDR(*PTE));
      memset(VA,1,PGSIZE); //fill with 1
      kfree(VA); //when freeing the physical page
      *PTE = 0;  //*PTE ^= PTE_P; ->this will work correctly maybe.
    }
  }
  return 1;
}

int freemem() {
  //이건 잘 모르겠는데요.
  return 0;
}

/*
                                                           :8DDDDDDDDDDDDDD$.                                           
                                                      DDDNNN8~~~~~~~~~~=~7DNNDNDDDNNI                                   
                                                  ?NNDD=~=~~~~~~~~~~~~~~~~~=~~==~=INNDNN7                               
                                               +NDDI~~~~~~~~~~~~~~~~~~~~~~~=~~========~ODND+                            
                                            :NND~~~~~~~~~~~~~~~~~~~~~~~~~~~=~~============7NDN                          
                                          $DD$~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=~~==============~DNN                        
                                        $DD=~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=~~=================NND                      
                                       ND7~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=~~===================DD7                    
                                     ~DD=~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=======================8DN.                  
                                    8DO~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=========================DD                  
                                   8N~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~=~~=======================DN                 
                                  NN::::::::~~~~~~~~~~~=~~~~~~~~~~~~~~~~~~~=~~========================DDO               
                                 $D$:::::::::::::::~~~~DD~~~~~~~~~~~~~~~~~~=~~=========================DN.              
                                 D8:::::::::::::::::::DN=::~~~~~~~~~~~~~~~~=~~======================~~:~DN              
                                DN:::::::::::::::::::ONO::::::::::::::::::::~~~~~~~~~~~~:::::::::::::::::DN             
                               DN::::::::::::::::::::NN.:::::::::::::::::::::::::::DN::::::::::::::::::::$DO            
                               DD:::::::::::::::::::DNI:::::::::::::::::::::::::::::D=::::::::::::::::::::NN            
                              NN~~~~:::::$N?:::::::.NN::::::::::::::::::::::::::::::ND.:::::::::::::::::::+N8           
                              N7~~~~~~~~OD7::::::::~DD::::::::::::::::::::::::::::::~D$::::::::::::::::::::DN           
                             NN~~~~~~~~IDZ~~~~~::::DN~:::::::::::::::::::::::::::::::DN::::::::::::::::::::=N~          
                             DD~~~~~~~~NN~~~~~~~~~=NN::::::::::::::::::::::::::::::::DN:::::::::::::::~~====NN          
                            8D~~~~~~~~ND~~~~~~~~~~~ND~~~~~~~~:::::::::::::::::::::::::N7:::~~===============NN          
                            DD~~~~~~~ON+~~~~~~~~~~~ND~~~~~~~~~~~~~~~~~~~=+NZ==========NN====================~ND         
               :DD7   DNDD. N8~~~~~~~NN~~~~~~~~~~DDND~~~~~~~~~~~~~~~~~~~~ND~~=========DD=====================ND         
               N~:DDNNN .8NDN~~~~~~~$D=~~~~~~~~+ND.DD~~~~~~~~~~~~~~~~~~~=DD~~=========~D+====================DN         
              :D     .  ..~ND~~~~~~~NN~~~~~~~+NN$..ND~~~~~~~~~~~~~~~~~~~7N=~~=========~ND=======~============ON         
              NN       ...:N?~~~~~~~N=~~~~~NNNI.. .7D+~~~~~~~~~~~~~~~~~=8NN~~==========NN=======N============$N         
         N  ODN       ....DN~~~~~~~DD=8NNND$..     .DD~~~=~~~~~~~~~~~~~=NNDD=~=========8D~======NN===========~N$        
    N? =NN  ND      .....NND~~~~~~~DDNN:...        .ND=~DNN~~~~~~~~~~~~=DN.DN~=========?N+======NN============ND        
   $D? DN    DZ    ....ND8NN~~~~~~$D                .DD~NNDD~~~~~~~~~~~~D8..DN=========~DN======NN============DN        
   DN ~N~   NN    ..:~NN..NZ~~~~~~DN                  NNN8.ND~~~~NDN?~~~DZ...7DD=======~NN======NN============DN        
   ND DD    :DN.  ..ND$  .N?~~~~~=NNN                   . ..DDD$~N8OND8=N+   ..DDDZ~====NN======+D+===========ND        
   NO         DD  ZDN    8NO~~~~~~NNN..DDDNN7               ...NND...:DDD:     .:.NDND=~DD======~DO===========DN        
              DNDDN:.    DN~~~~~~=NNNN.ODNNNNDDNNO              ...     .         ...DNNNN=======ND===========DD        
       INDN7    DD.     .DD~~~~~=IDND:.:~.....?DNDNN.                        ...... ....$D=======ND===========ND        
       NN        ND.    8N=~~~~$ND::.:=~:.~=......=ND~                 .NNNNNNNNNNNNNNN.~N+======NN===========DN        
       $DD        DN:   DD~~~~7NO...~==.:~~:.....                      NNNND? ..::..7NZ.:N?======8D~==========ZN        
       DN?     ~D: DND.?D~~~~~DD....~:.~=~.......                            ....~=:.:~..ND======~N$==========~DO       
       ND    ..DD.  .DNDN=~~~~DI.......:.........                           ....=~..~~~..DN======~DD===========NN       
       DDD  :.:DD.  . DDI~~~~~ND................        .DNNNNNNNNNN7      ....=~:.:~~...NN=======ND===========?D~      
       8D. ...OD..    DD~~~~~~+ND ............          NN:~::::~~~8N      ........~~...:ND=======DN============NN      
       DDI:...ND     .D7~~~~~~~7NN ..........           ID8::::::::8D      .............:DN=======ON============NN      
        ~NNND.N=.   .NN~~~~~~~~~NDN8                       ~::::::~N8       .............DN========D=============NI     
               DDNNN.ND~~~~~~~~DD =DND                                       ............DN========N+~===========NN     
                   ~:N=~~~~~~~~DD   .DDDD                                       ........ NN========DD============8D     
                    8N~~~~~~~~~ND    . .7NDDD? .                                      .8DDN========NN=============D:    
                    DD~~~~~~~~~DND:         IDNNND$.                           .+DNNNNDNIDN========DD=============DD    
                    ND~~~~~~~~ZN 7DD .. .:DDNDDNNDNNNNDDNDND8$?===+$8DDNNNDDDDDN8I~DN====8N========NN=============NN    
                    DD~~~~~~~~8N   DD.  .NN~~~~.~~=DNDNO.:7ODDDDNNDD8DDDND=~~~ =~~~ON====8N========DN=============DN    
                    ND~~~~~~~~DN    ZDD  DN~~~ ~~~~~=.7DDD+.......8NNN==~~~~~ ~~~~~ONN$==DN========8N=============ON    
                    ND~8N~=~~~ZN      DDODN=~.~~~~~=.~~~~INDNNNNDNN~~~~~~~~:~~~~~~~DN~ND=DN========DD=========~ND=8N    
                    IN=NDDI~~~~D8       DNN::~~~~~.~~~~~=.~~ND~~ND~~~~~~~~.~~~~~~~~NN  NDNN====ND==ND~D?======DNN=ND    
                     DNNI8ND=~~DN:       ZN=~~~~~ ~~~~~.~~~~DD~=DD~~~~~~~ ~~~~~~~=.ND. . ND===DNDD=NDDNN=====8NZDDDN    
                      NND  IDNDNNN+       D+~~~:~~~~~~ ~~~~~DDNNN+~~~~~~~~~~~~~~:=?N7   .ND=~ND  DNNN~ID====ND7 NNN     
                       ID                 ND~~ ~~~~~:.~~~7DDN7IDNN==~~ ~~~~~~~~ ~~DN   .:N?DDDDD NND  8N~=DDD  ZNN      
                                          NN~:~~~~~ =7DDDD+8N  :N8DDZ.~~~~~~~~.~~~DD.   NDD+ . DN=     OND+             
                                          DND~~~=8DNDDZ=~~ ND   NN~INND~~~~~.~~~~ND .    .    ..IDD                     
                                         DDNNNDNNN+~~~~~~.7N.    ND~~~NDDI~ ~~~~=NNN             .DDI                   
                                        DN=~~~~.=~~~~~~ ~~DN     +N+~~~~+DNDD~~~NNNND.            ..ND                  
                                         DDI~~ ~~~~~~~ ~~~ND..  ..ND~~~~:~~~DNDNNNN+            ..7O8ND+                
                                          .DND=~~~~=::~~=NN.   . . 8D~~.~~~~~~=DN$ODNDNDNNNDNNNNND8+~..                 
                                             8DNNI=.~~~~=NDDNNNNDDNDNN.~~~~~IDDNDND7:.                                  
                                                ?DNNDD?~DD          ~NN~~=NDD$                                          
                                                     :DDD.            NNNN=                                             
*/