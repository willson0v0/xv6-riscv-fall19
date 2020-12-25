#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmap.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

void*
try_fit(void* start, int length, struct proc* p)
{
  if(start > (void*)MAXVA) return 0;
  void* max_nxt = 0;
  void* end = start + length;
  int fail_flag = 0;
  
  // check overlap with vma
  for(int i = 0; i < MAX_VMA; i++)
  {
    if(p->vmas[i].valid)
    {
      void* pend = p->vmas[i].addr + p->vmas[i].length;

      if(max_nxt < pend && pend < start)
        max_nxt = pend;
      
      if(!((start > pend) || (end < p->vmas[i].addr)))  // overlap
        fail_flag = 1;
    }
  }

  for(uint64 ptr = (uint64)start; ptr < (uint64)start + length; ptr += PGSIZE)
  {
    if(*(walk(p->pagetable, ptr, 0)) & PTE_V)
      fail_flag = 1;
  }

  if(fail_flag)
  {
    if(start == max_nxt)
      max_nxt -= PGSIZE;
    else if(max_nxt == 0)
      max_nxt = start - PGSIZE;
    return try_fit(max_nxt, length, p);
  }
  else return start;
}

// mmap
uint64
sys_mmap(void)
{
  char* addr_discard; // dont care
  int length;
  int prot;
  int flags;
  int fd;
  struct file *f;
  int offset; // dont care

  if(argaddr(0, (uint64*)&addr_discard) < 0) return -1;
  if(argint(1, &length) < 0) return -1;
  if(argint(2, &prot) < 0) return -1;
  if(argint(3, &flags) < 0) return -1;
  if(argfd(4, &fd, &f) < 0) return -1;
  if(argint(5, &offset) < 0) return -1;

  // 0. assert not implemented features & check permission
  if(addr_discard != 0) panic("mmap addr");
  if(offset != 0) panic("mmap offset");
  if(!f->writable && (prot & PROT_WRITE) && (flags & MAP_SHARED)) return -1;

  // 1. allocate vma
  struct VMA* vma;
  for(vma = myproc()->vmas; vma < myproc()->vmas + MAX_VMA; vma++)
  {
    if(!vma->valid) break;
  }
  if(vma == myproc()->vmas + MAX_VMA) return -1;

  // 2. alloc from MAXVA
  // |     sz|           |existing vmas|  |MAXVA
  vma->addr = (void*)(TRAPFRAME - PGSIZE - length);
  while(1)
  {
    int fail_flag = 0;
    struct VMA* vma2;
    for(vma2 = myproc()->vmas; vma2 < myproc()->vmas + MAX_VMA && !fail_flag; vma2++)
    {
      if(vma2->valid)
      {
        if(vma->addr >= vma2->addr && vma->addr <= vma2->addr+vma2->length)
        {
          fail_flag = 1;
        }
        if(vma->addr + length >= vma2->addr && vma->addr + length <= vma2->addr+vma2->length)
        {
          fail_flag = 1;
        }
      }
    }
    if(fail_flag) vma->addr -= PGSIZE;
    else break;
  }
  if((uint64)vma->addr < myproc()->sz)
  {
    vma->valid = 0;
    return -1;
  }
  
  vma->valid = 1;
  vma->length = length;
  vma->prot = prot;
  vma->flags = flags;

  // 2. open and load file
  vma->f = f;
  filedup(f);

  return (uint64)vma->addr;
}

// munmap
uint64
sys_munmap(void)
{
  char* addr;
  int length;
  if(argaddr(0, (uint64*)&addr) < 0) return -1;
  if(argint(1, &length) < 0) return -1;

  // as told, test will only unmap head / tail, but not punch a hole
  
  // 1. find vma
  struct VMA* vma;
  for(vma = myproc()->vmas; vma < myproc()->vmas + MAX_VMA; vma++)
  {
    if(vma->valid && (char*)vma->addr <= addr && (char*)vma->addr + vma->length >= addr + length) break;
  }
  if(vma == myproc()->vmas + MAX_VMA) return -1;

  // 2. handle wb
  if(vma->flags & MAP_SHARED) // write back
    filewrite(vma->f, (uint64)addr, length);

  // 3. unmap user memory region. do free.
  uvmunmap(myproc()->pagetable, (uint64)addr, (uint64)length, 1);

  // 2. if is whole, remove vma; or shrink it instead.
  if(vma->addr == addr && vma->length == length)
  {
    fileclose(vma->f);
    vma->valid = 0;
  }
  else if(vma->addr == addr) // head;
  {
    vma->addr += length;
  }
  else  // tail
  {
    vma->length -= length;
  }


  return 0;
}