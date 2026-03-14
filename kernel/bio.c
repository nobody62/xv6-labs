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

#define NBUCKETS 13
#define HASH(n)  n % NBUCKETS

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct spinlock bktlocks[NBUCKETS];
  struct buf buckets[NBUCKETS];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i=0;i<NBUCKETS;++i){
    initlock(&bcache.bktlocks[i], "bcache.buckets");
    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].next;
    b->prev = &bcache.buckets[0];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].next->prev = b;
    bcache.buckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
#if 0
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}
#else
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *bb = 0;
  int tk = 0xffffff;
  int n = HASH(blockno);
  acquire(&bcache.bktlocks[n]);
  for(b = bcache.buckets[n].next;b != &bcache.buckets[n];b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bktlocks[n]);
      acquiresleep(&b->lock);
      return b;
    }
    else if(b->refcnt == 0 && b->ticks < tk){
      bb = b;
      tk = b->ticks;
    }
  }
  // not cached and this bucket is full
  if(!bb){
    acquire(&bcache.lock);
    for(int i=0;i<NBUF;++i){
      if(bcache.buf[i].refcnt == 0 && bcache.buf[i].ticks < tk){
        bb = &bcache.buf[i];
        tk = ticks;
      }
    }
    if(!bb){
      panic("bget: no buffers");
    }
    // change bucket
    bb->prev->next = bb->next;
    bb->next->prev = bb->prev;
    bb->prev = &bcache.buckets[n];
    bb->next = bcache.buckets[n].next;
    bcache.buckets[n].next->prev = bb;
    bcache.buckets[n].next = bb;
    release(&bcache.lock);
  }

  bb->dev = dev;
  bb->blockno = blockno;
  bb->valid = 0;
  bb->refcnt = 1;
  release(&bcache.bktlocks[n]);
  acquiresleep(&bb->lock);
  return bb;
}
#endif

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
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
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int n = HASH(b->blockno);

  acquire(&bcache.bktlocks[n]);
  b->refcnt--;
  acquire(&tickslock);
  b->ticks = ticks;
  release(&tickslock);
  release(&bcache.bktlocks[n]);
}

void
bpin(struct buf *b) {
  int n = HASH(b->blockno);
  acquire(&bcache.bktlocks[n]);
  b->refcnt++;
  release(&bcache.bktlocks[n]);
}

void
bunpin(struct buf *b) {
  int n = HASH(b->blockno);
  acquire(&bcache.bktlocks[n]);
  b->refcnt--;
  release(&bcache.bktlocks[n]);
}


