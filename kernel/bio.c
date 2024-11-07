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

#define NBUCKET 19

struct bucket
{
  struct spinlock lock; // 这个是桶的锁
  struct buf head;      // 这个是桶的链表头
}; // 包含互斥锁以及一个链表头

struct
{
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKET]; // 哈希桶
} bcache;

static uint hash_v(uint key)
{
  return key % NBUCKET;
} // 哈希映射

static void initbucket(struct bucket *b)
{
  initlock(&b->lock, "bcache.bucket");
  b->head.prev = &b->head;
  b->head.next = &b->head;
} // 初始化哈希桶

void binit(void)
{
  for (int i = 0; i < NBUF; ++i)
  {
    initsleeplock(&bcache.buf[i].lock, "buffer");
  } // 初始化所有缓冲区的锁
  for (int i = 0; i < NBUCKET; ++i)
  {
    initbucket(&bcache.bucket[i]);
  } // 初始化哈希桶
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  uint v = hash_v(blockno);                  // 先获取对应的哈希值
  struct bucket *bucket = &bcache.bucket[v]; // 获取对应的哈希桶
  acquire(&bucket->lock);                    // 桶上锁

  // Is the block already cached?
  for (struct buf *buf = bucket->head.next; buf != &bucket->head;
       buf = buf->next)
  {
    if (buf->dev == dev && buf->blockno == blockno)
    {
      buf->refcnt++;            // 引用计数加一
      release(&bucket->lock);   // 桶解锁
      acquiresleep(&buf->lock); // 该缓冲区上锁
      return buf;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (int i = 0; i < NBUF; ++i)
  {
    if (!bcache.buf[i].used &&
        !__atomic_test_and_set(&bcache.buf[i].used, __ATOMIC_ACQUIRE))
    {
      struct buf *buf = &bcache.buf[i]; // 获取缓冲区
      buf->dev = dev;
      buf->blockno = blockno;
      buf->valid = 0;
      buf->refcnt = 1;

      buf->next = bucket->head.next; // 将该缓冲区插入到对应桶的链表中
      buf->prev = &bucket->head;
      bucket->head.next->prev = buf;
      bucket->head.next = buf;
      release(&bucket->lock);   // 桶解锁
      acquiresleep(&buf->lock); // 该缓冲区上锁
      return buf;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint v = hash_v(b->blockno);
  struct bucket *bucket = &bcache.bucket[v];
  acquire(&bucket->lock);

  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    __atomic_clear(&b->used, __ATOMIC_RELEASE);
  }
  release(&bucket->lock);
}

void bpin(struct buf *b)
{
  uint v = hash_v(b->blockno);
  struct bucket *bucket = &bcache.bucket[v];
  acquire(&bucket->lock);
  b->refcnt++;
  release(&bucket->lock);
}

void bunpin(struct buf *b)
{
  uint v = hash_v(b->blockno);
  struct bucket *bucket = &bcache.bucket[v];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}