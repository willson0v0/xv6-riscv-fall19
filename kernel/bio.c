// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BCACHEHASHSIZE 17

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
  } bcache[BCACHEHASHSIZE];

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < BCACHEHASHSIZE; i++)
  {
    char name[9];
    strncpy(name, "bcache  ", 8);
    name[6] = (i/16 < 10) ? ('0' + i/16) : ('A' + i/16 - 10);
    name[7] = (i%16 < 10) ? ('0' + i%16) : ('A' + i%16 - 10);
    name[8] = '\0';
    initlock(&bcache[i].lock, name);
    

    // Create linked list of buffers
    bcache[i].head.prev = &bcache[i].head;
    bcache[i].head.next = &bcache[i].head;
    for(b = bcache[i].buf; b < bcache[i].buf+NBUF; b++){
      b->next = bcache[i].head.next;
      b->prev = &bcache[i].head;
      initsleeplock(&b->lock, name);
      bcache[i].head.next->prev = b;
      bcache[i].head.next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int hash = blockno % BCACHEHASHSIZE;

  acquire(&bcache[hash].lock);

  // Is the block already cached?
  for(b = bcache[hash].head.next; b != &bcache[hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.
  for(b = bcache[hash].head.prev; b != &bcache[hash].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int hash = b->blockno % BCACHEHASHSIZE;

  acquire(&bcache[hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache[hash].head.next;
    b->prev = &bcache[hash].head;
    bcache[hash].head.next->prev = b;
    bcache[hash].head.next = b;
  }
  
  release(&bcache[hash].lock);
}

void
bpin(struct buf *b) {
  int hash = b->blockno % BCACHEHASHSIZE;
  acquire(&bcache[hash].lock);
  b->refcnt++;
  release(&bcache[hash].lock);
}

void
bunpin(struct buf *b) {
  int hash = b->blockno % BCACHEHASHSIZE;
  acquire(&bcache[hash].lock);
  b->refcnt--;
  release(&bcache[hash].lock);
}


