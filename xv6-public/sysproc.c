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
    m->lowest_nice = 21;
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
    struct proc* p = myproc();

    //aquire lock
    while (m->locked != 0)
    {
        //check if lock fully aquired
        while (m->proc == 0x0)
        {
          yield();
        }
        acquire(&ptable.lock);

        //elevate nice of holding process
        if (p->nice < ((struct proc*)m->proc)->nice)
        {
          //cprintf("nice is:%d\n", p->nice);
          ((struct proc*)m->proc)->nice = p->nice;
        }

        //adjust lowest nice value in lock if needed
        if (m->lowest_nice > p->nice)
        {
          m->lowest_nice = p->nice;
        }

        //sleep thread
        sleep(m->_chan, &ptable.lock);
        release(&ptable.lock);
    }

    acquire(&ptable.lock);
    //record pid of holding process
    m->proc = (int)p;
    m->locked = 1;

    //add lock to process
    p->mutex[p->mutex_num] = (int)m;
    p->mutex_num++;

    //record old nice value
    m->old_nice = myproc()->nice;
    //cprintf("old_nice is:%d\n", myproc()->nice);
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
    struct proc* p = myproc();

    acquire(&ptable.lock);
    //remove lock from process
    int index = -1;
    for (int i = 0; i < p->mutex_num; i++)
    {
      if ((int)m == p->mutex[i])
      {
        index = i;
        continue;
      }
    }
    if (index == -1)
    {
      //error return
      cprintf("Lock not found!\n");
      return -1;
    }
    for (int i = index; i < p->mutex_num; i++)
    {
      if (i != p->mutex_num)
      {
        //shift
        p->mutex[i] = p->mutex[i+1];
      }
    }
    p->mutex_num--;

    //check if old nice is greater value than any other held lock
    int nice = m->old_nice;
    for (int i = 0; i < p->mutex_num; i++)
    {
      nice = (nice > ((mutex*)p->mutex[i])->lowest_nice) 
      ? ((mutex*)p->mutex[i])->lowest_nice : nice;
    }

    //adjust nice value to reflect waiting processes on still held locks
    p->nice = nice;
    //cprintf("rel_nice is:%d\n", p->nice);

    //reset lock
    m->lowest_nice = 21;
    m->proc = 0x0;
    m->locked = 0;
    release(&ptable.lock);
    //wakeup all waiting
    wakeup(m->_chan);

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

  int nice;
  //check if holding any locks
  if (p->mutex_num > 0)
  {
    //use old nice value
    nice = ((mutex*)p->mutex[0])->old_nice;
  }
  else //else use stored nice value
  {
    nice = p->nice;
  }

  //change nice value
  nice += inc;

  //check bounds and adjust
  if (nice > 19)
  {
    nice = 19;
  }
  else if (nice < -20)
  {
    nice = -20;
  }

  //check if new nice value lower than held nice value
  if (p->mutex_num > 0)
  {
    p->nice = (nice < p->nice) ? nice : p->nice;
  }
  else
  {
    p->nice = nice;
  }

  //adjust any locks that process holds
  for (int i = 0; i < p->mutex_num; i++)
  {
    ((mutex*)p->mutex[i])->old_nice = nice;
  }

  //release lock
  release(&ptable.lock);

  //return success
  return 0;
}
