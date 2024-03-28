#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "mutex.h"

//globals
extern Ptable ptable;
void* chan = (void*)0x1000;

int
sys_fork(void)
{
  return fork();
}

int sys_clone(void)
{
  int fn, stack, arg;
  argint(0, &fn);
  argint(1, &stack);
  argint(2, &arg);
  return clone((void (*)(void*))fn, (void*)stack, (void*)arg);
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  if (n == 0) {
    yield();
    return 0;
  }
  acquire(&tickslock);
  ticks0 = ticks;
  myproc()->sleepticks = n;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  myproc()->sleepticks = -1;
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/**
 * System call to initialize lock
*/
int sys_minit(void)
{
    //get mutex
    mutex* m;
    if (argint(0, (int*)&m) < 0)
    {
        //error return
        return -1;
    }

    acquire(&ptable.lock);
    m->locked = 0;
    m->_chan = chan;
    m->proc = 0x0;
    m->old_nice = 21;
    chan++;
    release(&ptable.lock);
    
    //return success
    return 0;
}

/**
 * System call to aquire lock
*/
int sys_macquire(void)
{
    //get mutex
    mutex* m;
    if (argint(0, (int*)&m) < 0)
    {
        //error return
        return -1;
    }

    //aquire lock
    while (xchg((volatile uint*)&(m->locked), 1) != 0)
    {
        //check if lock fully aquired
        while (m->proc == 0x0)
        {
          yield();
        }
        acquire(&ptable.lock);

        //elevate nice of holding process
        if (myproc()->nice < ((struct proc*)m->proc)->nice)
        {
          ((struct proc*)m->proc)->nice = myproc()->nice;
        }

        //sleep thread
        sleep(m->_chan, &ptable.lock);
        release(&ptable.lock);
    }

    acquire(&ptable.lock);
    //record pid of holding process
    m->proc = (int)myproc();

    //record old nice value
    m->old_nice = myproc()->nice;
    release(&ptable.lock);

    //return success
    return 0;
}

/**
 * System call to release lock
*/
int sys_mrelease(void)
{
    //get mutex
    mutex* m;
    if (argint(0, (int*)&m) < 0)
    {
        //error return
        return -1;
    }

    acquire(&ptable.lock);
    ((struct proc*)m->proc)->nice = m->old_nice;
    m->proc = 0x0;
    release(&ptable.lock);
    //wakeup all waiting
    wakeup(m->_chan);
    //unlock
    m->locked = 0;

    //return success
    return 0;
}

/**
 * System call to update nice value
*/
int sys_nice(void)
{
  //get inc val
  int inc;
  if (argint(0, &inc) < 0)
  {
    //error return
    return -1;
  }

  struct proc* p = myproc();

  //aquire lock so scheduler cannot prempt while changing nice value
  acquire(&ptable.lock);

  //change nice value
  p->nice += inc;

  //check bounds
  if (p->nice > 19)
  {
    p->nice = 19;
  }
  else if (p->nice < -20)
  {
    p->nice = -20;
  }

  //release lock
  release(&ptable.lock);

  //return success
  return 0;
}
