---
title: "6.S081 Lazy Lab"
date: 2020-11-13 20:10:14
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# Lazy Lab

> One of the many neat tricks an O/S can play with page table hardware is lazy allocation of user-space heap memory. Xv6 applications ask the kernel for heap memory using the sbrk() system call. In the kernel we've given you, sbrk() allocates physical memory and maps it into the process's virtual address space. It can take a long time for a kernel to allocate and map memory for a large request. Consider, for example, that a gigabyte consists of 262,144 4096-byte pages; that's a huge number of allocations even if each is individually cheap. In addition, some programs allocate more memory than they actually use (e.g., to implement sparse arrays), or allocate memory well in advance of use. To allow sbrk() to complete more quickly in these cases, sophisticated kernels allocate user memory lazily. That is, sbrk() doesn't allocate physical memory, but just remembers which user addresses are allocated and marks those addresses as invalid in the user page table. When the process first tries to use any given page of lazily-allocated memory, the CPU generates a page fault, which the kernel handles by allocating physical memory, zeroing it, and mapping it. You'll add this lazy allocation feature to xv6 in this lab.

# Eliminate allocation from sbrk()

* for the system call `sbrl` 
  * the positive argument , we simply increase the size of the process ,which means grows the address space ,but mark the addresses not valid in the page table , 
  * the negative argument , we directly  use the function `growproc `  ,which actually calls `uvmunmap`

```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(myproc()->originalsz==-1){
    myproc()->originalsz = addr;
  }
  if (n > 0)
  {
    myproc()->sz+=n;
  }
  else if (n < 0){
    if(growproc(n)<0)
     return -1;
  }
    
  // if(growproc(n) < 0)
  //   return -1;
  return addr;
}
```

# Lazy allocation

On a page fault on these not allocated address , the kernel allocates new pa and map into the page table.

```c
else if (r_scause() == 15 || r_scause() == 13)
  {
    uint64 va = r_stval();
    if (va >= p->sz)
    {
      p->killed = 1;
    }
    else if (va<p->originalsz-PGSIZE)
    {
      p->killed = 1;
    }
    else
    {
      //printf("page fault %p\n",va);
      char *mem;
      va = PGROUNDDOWN(va);
      mem = kalloc();
      if (mem == 0)
      {
        p->killed = 1;
      }
      else
      {
        memset(mem, 0, PGSIZE);
        mappages(p->pagetable, va, PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U);
      }
    }
  }
```

# Lazytests and Usertests

According to the hints!

> - Handle negative sbrk() arguments.
> - Kill a process if it page-faults on a virtual memory address higher than any allocated with sbrk().
> - Handle the parent-to-child memory copy in fork() correctly.
> - Handle the case in which a process passes a valid address from sbrk() to a system call such as read or write, but the memory for that address has not yet been allocated.
> - Handle out-of-memory correctly: if kalloc() fails in the page fault handler, kill the current process.
> - Handle faults on the invalid page below the user stack.

* The negative argument is a little tricky ,we directly call `growproc`
* higher va or lower va will be invalid , the higher va is simple to find ,the lower va is under the stack which are the guard page and text and  data 
* In `fork`  actually in `uvmcopy` , for the  not exist page and not valid mapping , we ignore !
* For the system call `read and write` , will happen “ page fault ” in `walkaddr` ,but not go to the `usertrap`,we need to handle it in `walkaddr`

```c
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0||(*pte & PTE_V) == 0)
  { 
    if (va >=myproc()->sz||va<myproc()->originalsz-PGSIZE)
    {
      pte = 0;
      return 0;
    }
    char *mem;
    va = PGROUNDDOWN(va); 
    mem = kalloc();
    if (mem == 0)
    {
      pte = 0;
      return 0;
    }
    else
    {
      memset(mem, 0, PGSIZE);
      mappages(pagetable, va, PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U);
      pte = walk(pagetable, va, 0);
    }
  } 
  // if ((*pte & PTE_V) == 0)
  //   return 0;
  if ((*pte & PTE_V) == 0||(*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}
```



.**The course lab site** :[MIT 6.S081 Lazy Lab](https://pdos.csail.mit.edu/6.828/2020/labs/lazy.html)

