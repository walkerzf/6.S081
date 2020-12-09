---
title: "6.S081 Cow Lab"
date: 2020-11-16 15:20:14
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# Cow Lab

Virtual memory provides a level of indirection: the kernel can intercept memory references by marking PTEs invalid or read-only, leading to **page faults**， and can change what addresses mean by modifying PTEs. There is a saying in computer systems that any systems problem can be solved with a level of indirection. **The lazy allocation lab provided one example which is talked about in  the last lab**. This lab explores another example: copy-on write fork.

# Fork’s Problem

The fork() system call in xv6 copies all of the parent process's user-space memory into the child. If the parent is large, copying can take a long time. Worse, the work is often largely wasted; for example, a fork() followed by exec() in the child will cause the child to discard the copied memory, probably without ever using most of it. On the other hand, if both parent and child use a page, and one or both writes it, a copy is truly needed.

# Implementation

The goal of copy-on-write (COW) fork() is to defer allocating and copying physical memory pages for the child until the copies are actually needed, if ever.

`Cow` fork will creates just a page table for the child , with PTEs for user memory pointing to the parent’s  `pa`. `Cow` fork will marks all the user PTEs in both parent and child as not writable. When either process wants to write any of these unwritable pages, will triggle a page fault .The kernel trap will handle this fault , allocates a page of physical memory for the page fault,copies the original page into the new page, and **modifies the relevant PTE in the faulting process to refer to the new page**, this time with the PTE marked writeable. The original pa will be not changed.

`uvmcopy` we will not allocate new pages , we increase the `refcnt`  for the pa.

## `Uvmcopy` not allocate the new pa 

```c
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for (i = 0; i < sz; i += PGSIZE)
  {
    if ((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    //fix the permission bits
    pa = PTE2PA(*pte);
    *pte &= ~PTE_W;
    flags = PTE_FLAGS(*pte);
	//not allocated
    // if((mem = kalloc()) == 0)
    //   goto err;
    // memmove(mem, (char*)pa, PGSIZE);
	//increase refcnt
    increse(pa);
    //map the va to the same pa using flags
    if (mappages(new, i, PGSIZE, (uint64)pa, flags) != 0)
    {
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

## `Kalloc` `Kfree` ` increase cnt`

`kalloc` , we maintain the `refcnt` for every physical page .In the initialization , the `refcnt` will be `writed `to 1,because  in the `freerange` ,we call `kfree` which decreases the `refcnt` for every `pa`.

```c
int refcnt[PHYSTOP / PGSIZE];
void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    refcnt[(uint64)p / PGSIZE] = 1;
    kfree(p);
  }
}
```

`increase refcnt` and `kfree` is a combination , which is increase the `refcnt` of the `pa` , the other is decrease the `refcnt` of the pa .In the case when the `refcnt` of the `pa` down to zero , we really free the `pa`!

```c
void increse(uint64 pa)
{ 
    //acquire the lock
  acquire(&kmem.lock);
  int pn = pa / PGSIZE;
  if(pa>PHYSTOP || refcnt[pn]<1){
    panic("increase ref cnt");
  }
  refcnt[pn]++;
  release(&kmem.lock);
}

void kfree(void *pa)
{
  struct run *r;
  r = (struct run *)pa;
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
	//when we free the page decraese the refcnt of the pa 
    //we need to acquire the lock
    //and get the really current cnt for the current fucntion
  acquire(&kmem.lock);
  int pn = (uint64)r / PGSIZE;
  if (refcnt[pn] < 1)
    panic("kfree panic");
  refcnt[pn] -= 1;
  int tmp = refcnt[pn];
  release(&kmem.lock);

  if (tmp >0)
    return;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```

`kalloc` function will allocate a `pa` ,  if the `pa` ref cnt is not valid , `panic`.

```c
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;

  if (r)
  {
    int pn = (uint64)r / PGSIZE;
    if(refcnt[pn]!=0){
      panic("refcnt kalloc");
    }
    refcnt[pn] = 1;
    kmem.freelist = r->next;
  }

  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}
```

## Handle function in Trap

The `r_scause` of page fault is 15 or 13. In `usertrap` , we have `cowfault`.

```c
 if (r_scause() == 15)
  {
    if ((cowfault(p->pagetable, r_stval()) )< 0)
    {
      p->killed = 1;
    }
  }
```

**`cowfault` function**

* handle the  invalid `va` 
  * more than `MAXVA`
  * not in the page table
  * not set user bit or valid bit
* allocate a new `pa` , copy the original content to the new `pa` ,  
  * `unmap` and  `map` for this `pte` entry!
  * or we cook up  the `pte `straightly

```c
int cowfault(pagetable_t pagetable, uint64 va)
{
  if (va >= MAXVA)
    return -1;
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return -1;
  if ((*pte & PTE_U) == 0 || (*pte & PTE_V) == 0)
    return -1;
  uint64 pa1 = PTE2PA(*pte);
  uint64 pa2 = (uint64)kalloc();
  if (pa2 == 0){
    //panic("cow panic kalloc");
    return -1;
  }
 
  memmove((void *)pa2, (void *)pa1, PGSIZE);
  *pte = PA2PTE(pa2) | PTE_U | PTE_V | PTE_W | PTE_X|PTE_R;
   kfree((void *)pa1);
  return 0;
}
```

One more thing ,according to the hint :  Modify `copyout()` to use the same scheme as page faults when it encounters a COW page.

```c
va0 = PGROUNDDOWN(dstva);
if (va0 > MAXVA)
    return -1;    
if(cowfault(pagetable,va0)<0){
	return -1;
} 
```

.**The course lab site** :[MIT 6.S081 cow Lab](https://pdos.csail.mit.edu/6.828/2020/labs/cow.html)

