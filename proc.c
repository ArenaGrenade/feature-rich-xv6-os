#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

Queue *queues[MLFQSIZE];

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");

  #ifdef MLFQ
    // Initializes all the queues

    acquire(&ptable.lock);

    int id_count = 0;
    cprintf("Allocating the queues\n");
    for (int i = 0; i < MLFQSIZE; i++) {
      // queues[i]->rear = queues[i]->front = -1;
      queues[i]->queue_id = id_count++;
      cprintf("Allocated queue %d\n", id_count - 1);
    }
    release(&ptable.lock);
    cprintf("Allocated all queues\n");
  #endif
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
static struct proc*
allocproc(void)
{
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

  acquire(&ptable.lock);
  // Set the process start time here.
  p->ctime = ticks;

  // Set all default values at the start
  p->etime = 0;
  p->rtime = 0;
  p->wtime = 0;
  p->iotime = 0;
  p->n_shed = 0;
  p->punish = 0;
  p->time_slices = 0;

  // Set default priority based on process
  if (p->pid == 1 || p->pid == 2) 
    p->priority = 1;
  else
    p->priority = 60;
  release(&ptable.lock);

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

  #ifdef RR
    cprintf("\n\nUsing standard round-robin scheduler\n\n");
  #else
  #ifdef FCFS
    cprintf("\n\nUsing FCFS scheduler\n\n");
  #else
  #ifdef PBS
    cprintf("\n\nUsing Priority based Scheduler\n\n");
  #else
  #ifdef MLFQ
    cprintf("On MLFQ init proc\n");
    acquire(&ptable.lock);
    push(queues[0], p);
    cprintf("Passed push\n");
    p->cur_queue = 0;
    release(&ptable.lock);
    display(queues[0]);
    cprintf("\n\nUsing Multi-level Feedback Queue Scheduler\n\n");
  #endif
  #endif
  #endif
  #endif
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
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
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

  #ifdef MLFQ
    acquire(&ptable.lock);
    push(queues[0], np);
    np->cur_queue = 0;
    release(&ptable.lock);
    display(queues[0]);
    cprintf("\n\nUsing Multi-level Feedback Queue Scheduler\n\n");
  #endif

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

  // Mark the end time of a process
  curproc->etime = ticks;
  // cprintf("Process end time is %d\n", curproc->etime);
  // ps();
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

// Waitx system call and is an exact copy of the wait call
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
waitx(uint* wtime, uint* rtime)
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
        // Assign the wait times and run times to the variable provided to us
        cprintf(
          "\nProcess started at %d and ran for %d, waited for %d, slept for %d and ended at %d and entered scheduler for %d times with %d cpus\n", 
          p->ctime, p->rtime, p->wtime / ncpu, p->iotime / ncpu, p->etime, p->n_shed, ncpu
        );
        // cprintf("This process had priority %d\n", p->priority);
        *wtime = p->wtime / ncpu;
        *rtime = p->rtime;

        // Found one.
        pid = p->pid;
        kfree(p->kstack); 
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        // p->state = UNUSED;
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

// Updates the timing vales for all processes in the proc table
// for every clock tick - might not be desired
void
update_timing() {
  acquire(&ptable.lock);

  for(struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == RUNNING) p->rtime++;
    else if (p->state == RUNNABLE) p->wtime++;
    else if (p->state == SLEEPING) p->iotime++;
  }

  release(&ptable.lock);
}

// Set process priority
int
set_priority(int new_priority, int pid) {
  struct proc *p;
  sti(); // Enable interrupts

  if(new_priority < 0 || new_priority > 100) {
    cprintf("Out of bounds priority. Priority can only be between 0 and 100.\n");
    return -1;
  }

  int old_priority = -1;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      old_priority = p->priority;
      p->priority = new_priority;
      break;
    }
  }
  release(&ptable.lock);

  if (old_priority == -1 || p->priority != new_priority) 
    return -1;
  
  if (myproc()->pid == pid)
    yield();

  // cprintf("Process %d with a priority %d has been given a priority of %d\n", pid, old_priority, new_priority);
  return old_priority;
}

// Punisher
void punisher() {
  acquire(&ptable.lock);
  myproc()->punish = 1;
  release(&ptable.lock);
}

// Increase time slice of process
void inc_timeslice() {
  acquire(&ptable.lock);
  myproc()->time_slices++;
  release(&ptable.lock);
}

// Process Ager - this basically looks at all the processes and ages them accordingly.
// returns if the queue was found empty or not
int age_processes(int queue_id) {
  // cprintf("We are ageing the queue %d\n", queue_id);
  if (queues[queue_id]->front == -1 && queues[queue_id]->rear == -1) return 1;
  else {
    // Code for ageing here.
    return 0;
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
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  #if defined(PBS) || defined(FCFS)
    struct proc* p1;
  #endif

  #ifdef MLFQ
    while (1) {
      sti();

      acquire(&ptable.lock);

      for (int i = 1; i < MLFQSIZE; i++)
        age_processes(i);
      
      p = 0;
      for (int i = 0; i < MLFQSIZE; i++) {
        if (get_size(queues[i]) == 0) {
          continue;
        }
        p = queues[i]->arr[0];
        pop(queues[i]);
        break;
      }

      if (p == 0 || p->state != RUNNABLE) {
        release(&ptable.lock);
        continue;
      }

      cprintf("Sending process %d to run\n", p->pid);

      p->time_slices++;
      p->n_shed++;

      // Switch to chosen process. It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      // Back from p
      if (p != 0 && p->state == RUNNABLE) {
        if (p->punish == 0) {
          p->time_slices = 0;
          // p->age_time = ticks;
        } else {
          p->time_slices = 0;
          p->punish = 0;
          // p->age_time = ticks;
          
          if (p->cur_queue != MLFQSIZE - 1)
            p->cur_queue++;
        }
        push(queues[p->cur_queue], p);
      }

      release(&ptable.lock);
    }
  #else  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if (p->state != RUNNABLE)
        continue;
      #ifndef RR
      struct proc* chosen_proc = p;
      for (p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++) {
        if (p1->state != RUNNABLE)
          continue;

        #ifdef FCFS
        if (p1->ctime < chosen_proc->ctime)
          chosen_proc = p1; 
        #else   
        #ifdef PBS
        if (p1->priority < chosen_proc->priority)
          chosen_proc = p1;
        #endif
        #endif
      }
      p = chosen_proc;
      #endif

      p->n_shed++;
      p->time_slices++;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
  #endif
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
void
yield(void)
{
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
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
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

// ps command that lists process details as a table
int
ps(void) {
  struct proc *p;
  sti();

  cprintf("PID \t Prior \t State \t rtime \t wtime \t nruns \t curq \t q0 \t q1 \t q2 \t q3 \t q4\n");

  static char *states[] = {
    [EMBRYO]    "EMBRYO",
    [SLEEPING]  "SLEEP",
    [RUNNABLE]  "WAIT",
    [RUNNING]   "RUN",
    [ZOMBIE]    "ZOMB"
  };

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED) 
      continue;

    cprintf("%d \t %d \t %s \t", p->pid, p->priority, states[p->state]);
    cprintf(" %d \t %d \t %d\n", p->rtime, p->wtime / ncpu, p->n_shed);
  }
  release(&ptable.lock);

  return 0;
}
