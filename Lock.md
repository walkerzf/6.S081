---
title: 6.S081 Lock Lab
date: 2020-11-19 15:20:00
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# Lock Lab

In this lab you'll gain experience in re-designing code to increase parallelism. A common symptom of poor parallelism on multi-core machines is high lock contention. Improving parallelism often involves changing both data structures and locking strategies in order to reduce contention. You'll do this for the xv6 memory allocator and block cache.

# Memory allocator

It is straightforward to  assign each lock to each CPU ‘s  `freelist`.

* The basic idea is to maintain a free list per CPU, each list with its own lock.

- Let `freerange` give all free memory to the CPU running `freerange`.
- The function `cpuid` returns the current core number, but it's only safe to call it and use its result when interrupts are turned off. You should use `push_off()` and `pop_off()` to turn interrupts off and on.

Redesign struct` kmem[NCPU]` .

```c
struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];
```

Assign all free page to the CPU which calls the `freerange`.

```c
void freerange(void *pa_start, void *pa_end)
{
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE){
    memset(p, 1, PGSIZE);
    struct run *r = (struct run *)p;
    
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    
  }
  release(&kmem[id].lock);
  pop_off();
}
```

`Kfree` and `Kalloc` will relate to the which CPU calls the function.

```c
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  pop_off();
  release(&kmem[id].lock);
  
}

void *
kalloc(void)
{
  struct run *r;
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);

  r = kmem[id].freelist;
  if (r)
    kmem[id].freelist = r->next;
  else
  {
    for (int i = 0; i < NCPU; i++)
    {
      if (i == id)
        continue;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if (r)
      {	
         //remember to release
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }else{
          //remember to release
        release(&kmem[i].lock);
      }
    }
  }
  
  release(&kmem[id].lock);
  pop_off();
  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
```

# Buffer cache

This is similar to the `Memory allocator ` , but we can not assign a bucket for each CPU , because these buffers are shared among each CPU or each Process.

According to the hint , we remove the doubly-linked list , add two filed `used` (means the buffer is inuse or free) and `timestamp` to recycle a buffer.

`Struct BUF`

```c
struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  int used;
  uint timestamp;
};
```

`bcache` Bucket . `helper lock` helps the  serialize eviction in `Bget`.

```
#define NBUCKET 13
struct spinlock helperlock;
struct
{
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache[NBUCKET];
```

`Bget`  function 

```c

static struct buf *bget(uint dev, uint blockno)
{
  
  acquire(&helperlock);
  struct buf *b;
  int id = blockno % NBUCKET;
  acquire(&bcache[id].lock);
  // Is the block already cached?
  for (int i = 0; i < NBUF; i++)
  {
    b = &bcache[id].buf[i];
    if (b->dev == dev && b->blockno == blockno && b->used)
    {
      b->refcnt++;
      b->timestamp = ticks;
      release(&bcache[id].lock);
      // pop_off();
      release(&helperlock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //what we need to do is to find a buffer index in index bucket
  //and recycle one buf whose timestamp is samllest and refcnt == 0 and active.!
  struct buf *dst = 0, *src = 0;
  uint time = 0x7fffffff;
  for (int i = 0; i < NBUF; i++)
  {
    if (bcache[id].buf[i].used == 0)
    {
      dst = &bcache[id].buf[i];
      break;
    }
  }
  int pre = -1;
  //search a smallest timestamp
  for (int i = 0; i < NBUCKET; i++)
  {
    if (i != id)
    {
      acquire(&bcache[i].lock);
    }
    int flag = 0;
    
    for (int j = 0; j < NBUF; j++)
    {
      b = &bcache[i].buf[j];
      if (b->used == 1 && b->refcnt == 0 && b->timestamp < time)
      {
        time = b->timestamp;
        src = b;
        flag = 1;
      }
    }
    if (flag)
    {
      if (pre == -1)
        pre = i;
      else
      {
        if (pre != id)
          release(&bcache[pre].lock);
        pre = i;
      }
    }
    else if (i != id)
    {
      release(&bcache[i].lock);
    }
  }
  if (src == 0)
    panic("No buffer!\n");

  //get the dst and src
  //implement  replace
  if (dst == 0)
  {
    dst = src;
  }
  src->used = 0;
  dst->used = 1;
  dst->dev = dev;
  dst->blockno = blockno;
  dst->used = 1;
  dst->valid = 0;
  dst->refcnt = 1;
  
  release(&bcache[id].lock);
  if(pre!=id) release(&bcache[pre].lock);
  release(&helperlock);
  acquiresleep(&dst->lock);
  return dst;
}
```

`brelse` `bpin` `bunpin` function

```c

int findbucket(struct buf *b)
{
  for (int i = 0; i < NBUCKET; i++)
  {
    if (b >= bcache[i].buf && b <= (bcache[i].buf + NBUF))
    {
      return i;
    }
  }
  panic("findbucket");
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
// we have the structure in the bucket , no need to move the buffer!
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int id = findbucket(b);
  acquire(&bcache[id].lock);
  b->refcnt--;
  release(&bcache[id].lock);
}

void bpin(struct buf *b)
{
  int id = findbucket(b);
  acquire(&bcache[id].lock);
  b->refcnt++;
  release(&bcache[id].lock);
}

void bunpin(struct buf *b)
{
  int id = findbucket(b);
  acquire(&bcache[id].lock);
  b->refcnt--;
  release(&bcache[id].lock);
}
```



# Link

[Link to Lock Lab Page](https://pdos.csail.mit.edu/6.828/2020/labs/lock.html)