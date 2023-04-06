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

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;
struct hashbuf{
  struct spinlock lock;
  struct buf head;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct hashbuf buckets[13];
}bcache;

// // 将buf数组依次以头插法插入到bcache.head链表中
// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

//   // Create linked list of buffers
//   bcache.head.prev = &bcache.head;
//   bcache.head.next = &bcache.head;
//   for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     initsleeplock(&b->lock, "buffer");
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
// }
void 
binit(void){
  initlock(&bcache.lock, "bcache");
  struct buf *b;
  for(int i=0;i<13;i++){
    char name[9];
    snprintf(name, sizeof(name), "bcache%d",i);
    initlock(&bcache.buckets->lock,name);
    bcache.buckets[i].head.next=&bcache.buckets[i].head;
    bcache.buckets[i].head.prev=&bcache.buckets[i].head;
  }
  for(b=bcache.buf;b<bcache.buf+NBUF;b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// // Look through buffer cache for block on device dev.
// // If not found, allocate a buffer.
// // In either case, return locked buffer.
// static struct buf*
// bget(uint dev, uint blockno)
// {
//   struct buf *b;

//   acquire(&bcache.lock);

//   // Is the block already cached?
//   for(b = bcache.head.next; b != &bcache.head; b = b->next){
//     if(b->dev == dev && b->blockno == blockno){
//       b->refcnt++;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }

//   // Not cached.
//   // Recycle the least recently used (LRU) unused buffer.
//   for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//     if(b->refcnt == 0) {
//       b->dev = dev;
//       b->blockno = blockno;
//       b->valid = 0;
//       b->refcnt = 1;
//       release(&bcache.lock);
//       acquiresleep(&b->lock);
//       return b;
//     }
//   }
//   panic("bget: no buffers");
// }

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bid=blockno%13;
  acquire(&bcache.buckets[bid].lock);
  for(b = bcache.buckets[bid].head.next; b != &bcache.buckets[bid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp=ticks;
      release(&tickslock);
      release(&bcache.buckets[bid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  for(int i=bid,cycle=0;cycle!=13;i=(i+1)%13,cycle++){
    struct buf *min=0;
    if(i!=bid){
      if(!holding(&bcache.buckets[i].lock))
        acquire(&bcache.buckets[i].lock);
      else
        continue;
      // acquire(&bcache.buckets[i].lock);
    }
    for(b = bcache.buckets[i].head.next; b != &bcache.buckets[i].head; b = b->next){
      if(b->refcnt==0&&(min==0||b->timestamp<min->timestamp)){
        min=b;
      }
    }
    // 如果当前桶没有找到直接释放锁
    if(min==0){
      if(i!=bid)
        release(&bcache.buckets[i].lock);
    }
    else{
      // 如果是从别的桶窃取的采用头插法插入bid桶里
      if(i!=bid){
        min->prev->next=min->next;
        min->next->prev=min->prev;
        release(&bcache.buckets[i].lock);
        min->next = bcache.buckets[bid].head.next;
        min->prev = &bcache.buckets[bid].head;
        bcache.buckets[bid].head.next->prev = min;
        bcache.buckets[bid].head.next = min;
      }
      acquire(&tickslock);
      min->timestamp=ticks;
      release(&tickslock);
      min->dev = dev;
      min->blockno = blockno;
      min->valid = 0;
      min->refcnt = 1;
      release(&bcache.buckets[bid].lock);
      acquiresleep(&min->lock);
      return min;
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

// // Release a locked buffer.
// // Move to the head of the most-recently-used list.
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock);

//   acquire(&bcache.lock);
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
  
//   release(&bcache.lock);
// }

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int bid=b->blockno%13;
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  acquire(&tickslock);
  b->timestamp=ticks;
  release(&tickslock);
  release(&bcache.buckets[bid].lock);
}

void
bpin(struct buf *b) {
  int bid=b->blockno%13;
  acquire(&bcache.buckets[bid].lock);
  b->refcnt++;
  release(&bcache.buckets[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid=b->blockno%13;
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  release(&bcache.buckets[bid].lock);
}


