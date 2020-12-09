---
title: 6.S081 mmap Lab
date: 2020-11-22 23:10:00
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# mmap Lab

The `mmap` and `munmap` system calls allow UNIX programs to exert detailed control over their address spaces. They can be used to share memory among processes, to map files into process address spaces,but this lab requires only a subset of its features relevant to memory-mapping a file

#  `mmap`

Keep track of what `mmap` has mapped for each process. Define a structure corresponding to the VMA (virtual memory area) ,recording the address, length, permissions, file, etc. for a virtual memory range created by `mmap` .The VMA should contain a pointer to a `struct file` for the file being mapped; `mmap` should increase the file's reference count so that the structure doesn't disappear when the file is closed (hint: see `filedup`). 

`proc.h` : one more field.

```c
struct VMA vma[16];
struct VMA{
  uint64 addr;	// the start address of va 
  uint64 end;	//the end address of va
  int prot;		// the prot permission
  int flags;	//map shared or privaet
  int fd;		//file descriptor
  int offset;	//the offset of the file  = 0
  struct file *pf;	// the pointer to the file struct
  int used;		//indicates this vma is used or not used
};
```

`sys_mmap` : 

* get the system call argument value using the helper function ,` argfd` is very interesting
* the file is not writable but using `prot  PROT_WRITE` and `MAP_SHARED` should be a failed `mmap `
* get a free `vma` field to store the value  , lazy increase the size of the user process , So we ensure that `mmap` of a large file is fast, and that `mmap` of a file larger than physical memory is possible.

```c
uint64
sys_mmap(void)
{
  struct proc *p = myproc();
  // void * addr  uint  length  int  prot  int  flags  int  fd  int  offset
  //addr will always be zero
  // offset will be zero
  int length;
  int prot, flags, fd;
  struct file *f;
  if (argint(1, &length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 || argfd(4, &fd, &f) < 0)
    return 0xffffffffffffffff;
  //not allocate the pa and not really read the file for read the large file fast and large file possible
  //find a a,ddress
  if(!f->writable&&(prot&PROT_WRITE)&&flags == MAP_SHARED) return -1;
  if (p->originalsize == -1)
    p->originalsize = p->sz;
  int index = 0;
  for (; index < 16; index++)
  {
    if (p->vma[index].used == 0)
      break;
  }
  printf("%d\n", index);
  if (index != 16)
  {
    filedup(f);
    //find an empty but page -aligned
    uint64 va = (p->sz);
    p->sz = va + length;
    //create a new entry in struct
    p->vma[index].addr = va;
    p->vma[index].end = PGROUNDUP(va + length);
    p->vma[index].prot = prot;
    p->vma[index].flags = flags;
    //p->vma[index].fd = fd;
    p->vma[index].offset = 0;
    p->vma[index].pf = f;

    p->vma[index].used = 1;
    return va;
  }
  else
    return 0xffffffffffffffff;
}
```

# `usertrap`

The page  fault handler in `trap.c`. The structure is similar to the `cowfault`!

```c
 else if (r_scause() == 15 || r_scause() == 13)
  {

    if (helper(p->pagetable, r_stval()) < 0)
    {
      p->killed = 1;
    }
  }
```

* The `va` is no valid should kill the process
* Or we need to find the corresponding  `vma`  to know the start address address and the end address and `prot` (the name of variable makes sense plz! It makes me spend more three hours ,using `gdb ` to debug plz!)
* read the file content and add mapping in `helper` function .The offset of the file depends on the `va` and the start of the mapping because the offset = 0  .The mapping always be `PGSIZE`.
* `readi` needs the lock the `inode` of the file 

```c
int helper(pagetable_t pagetable, uint64 va)
{
  struct proc *p = myproc();
  if (va > MAXVA || va >= p->sz)
    return -1;
  va = PGROUNDDOWN(va);
  int index = 0;
  for (; index < 16; index++)
  {
    //printf("%d\n",p->vma[index].used);
    if (p->vma[index].used == 1 && va >= p->vma[index].addr && va < p->vma[index].end)
    {
      break;
    }
  }
  /**
  !!!!!! the name of variable make sense plz!!!!
  */
  //printf("%d!\n", index);
  if (index == 16)
    return -1;
  //get the index
  uint64 addr = p->vma[index].addr;
  //int length = p->vma[index].length;
  //int prot = p->vma[index].prot;
  int prot = p->vma[index].prot;
  //int fd = p->vma[index].fd;
  //int offset = p->vma[index].offset;
  struct file *pf = p->vma[index].pf;

  //allocate the pa and map
  char *mem;
  mem = (char *)kalloc();

  if (mem == 0)
    return -1;
  //read the file date into the pa
  memset(mem, 0, PGSIZE);
  begin_op();
  ilock(pf->ip);
  if (readi(pf->ip, 0, (uint64)mem, va - addr, PGSIZE) < 0)
  {
    iunlock(pf->ip);
    end_op();
    return -1;
  }
  iunlock(pf->ip);
  end_op();
  //printf("%s\n",mem[0]);
  uint64 f = PTE_U;
  if (prot & PROT_EXEC)
    f |= PTE_X;
  if (prot & PROT_READ)
    f |= PTE_R;
  if (prot & PROT_WRITE)
    f |= PTE_W;

  if (mappages(pagetable, va, PGSIZE, (uint64)mem, f) != 0)
  {
    kfree(mem);
    return -1;
  }
  return 0;
}
```

#  `unmmap`

We need tot  find the VMA for the address range and `unmap` the specified pages  using  `uvmunmap`. Our own `unmmap` only writes back the file is mapped `MAP_SHARED ` .Because the test does  not check that not dirty bit are not written back; thus you can get away with writing pages back without looking at `D` bits. 

In the test , we only `unmmap ` the part in the beginning of the file or at the end of the file. So we do not need to dup a new `vma` for the `unmmap`. 

* according to the address , we find the `vma` 
* check the bit of the `flags` of map , to decide whether write back or not 
  * write back using `writei` , writing to the suitable offset of the file 
* using `unmunmap` to help `unmap` the mapping in the page table
* do some change to the `vma`  , if the whole region of file is unmapped , we decrease the `refcnt` of the file using the helper function in `file.c`
* Modify `fork` to ensure that the child has the same mapped regions as the parent. Don't forget to increment the reference count for a VMA's `struct file`

```c
uint64
sys_munmap(void)
{
  struct proc *p = myproc();
  uint64 addr;
  int len;
  if (argaddr(0, &addr) < 0 || argint(1, &len) < 0)
    return -1;
  int index = 0;
  for (; index < 16; index++)
  {
    if (p->vma[index].used == 1 && p->vma[index].addr <= addr 
        &&addr<p->vma[index].end)
    { 
      //according to the test case to code  
      if(p->vma[index].flags==MAP_SHARED){
        begin_op();
        ilock(p->vma[index].pf->ip);
        writei(p->vma[index].pf->ip,1,addr,addr - p->vma[index].addr,len);
        iunlock(p->vma[index].pf->ip);
        end_op();
      }
      uvmunmap(p->pagetable,addr,len/PGSIZE,1);
      //in the former part
      //not in the middle part
      if(addr==p->vma[index].addr){
        if(p->vma[index].end==addr+len){
          //decrease the filecnt
          fileddown(p->vma[index].pf);
          p->vma[index].used = 0;
        }else{
            p->vma[index].addr = addr+len;
        }
      }else if( addr+len == p->vma[index].end){
        p->vma[index].end = addr+len;
      }
      //the sub of one mmap
      return 0;
    }
  }

  return -1;
}
```

`fork`

```c
  for(int i = 0;i<16;i++){
    np->vma[i].addr = p->vma[i].addr;
    np->vma[i].end = p->vma[i].end;
    np->vma[i].prot = p->vma[i].prot;
    np->vma[i].flags = p->vma[i].flags;
    np->vma[i].used = p->vma[i].used;
    if((np->vma[i].pf = p->vma[i].pf)!=0){
      filedup(np->vma[i].pf);
    }
  }
//and in the `uvmcopy`
if((pte = walk(old, i, 0)) == 0)
      continue;
      //panic("uvmcopy: pte should exist");
if((*pte & PTE_V) == 0)
      continue;
      //panic("uvmcopy: page not present");
```

`filedown` similar to `filedup`

```c
// decrease ref count for file f.
struct file*
fileddown(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileddown");
  f->ref--;
  release(&ftable.lock);
  return f;
}
```

# Link

[Link to mmap  Lab Page](https://pdos.csail.mit.edu/6.828/2020/labs/mmap.html)