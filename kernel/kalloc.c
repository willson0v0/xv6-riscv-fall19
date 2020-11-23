// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  char lockName[7];
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
  {
    strncpy(kmem[i].lockName, "kmem  ", 7);
    kmem[i].lockName[4] = (i/16) > 9 ? (i/16 - 10 + 'A') : (i/16 + '0');
    kmem[i].lockName[5] = (i%16) > 9 ? (i%16 - 10 + 'A') : (i%16 + '0');
    initlock(&kmem[i].lock, kmem[i].lockName);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  push_off();
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    kfree(p);
  }
  pop_off();
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  acquire(&kmem[cpuid()].lock);
  r->next = kmem[cpuid()].freelist;
  kmem[cpuid()].freelist = r;
  release(&kmem[cpuid()].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  acquire(&kmem[cpuid()].lock);
  r = kmem[cpuid()].freelist;
  if(r)
  {
    kmem[cpuid()].freelist = r->next;
    release(&kmem[cpuid()].lock);
  }
  else
  {
    release(&kmem[cpuid()].lock);
    for(int i = 0; i < NCPU; i++)
    {
      if(i == cpuid()) continue;
      acquire(&kmem[i].lock);
      struct run *r2 = kmem[i].freelist;
      if(r2)
      {
        kmem[i].freelist = r2->next;
        r = r2;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
