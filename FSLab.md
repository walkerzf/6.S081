---
title: 6.S081 FS Lab
date: 2020-11-20 11:10:00
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# FS Lab

In this lab you will add large files and symbolic links to the xv6 file system.

In the first part ,  you will make the max size of a file in `xv6` much bigger through sacrifice a direct block and adding a doubly-indirect block. In the second part , you will add symbolic link to the file in `xv6` .Symbolic links resembles hard links, but hard links are restricted to pointing to file on the same disk, while symbolic links can cross disk devices. This is a good exercise to know about the pathname lookup in xv6

# Part 1 Large files

The first 11 elements of `ip->addrs[]` should be direct blocks; the 12th should be a singly-indirect block (just like the current one); the 13th should be your new doubly-indirect block. You are done with this exercise when `bigfile` writes 65803 blocks .

```c
#define NDIRECT 11
#define DDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
struct inode {
  //...//
  uint addrs[NDIRECT+1+1];
};
```

`bmap` function need to handle the doubly-indirect block

```c
 uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if (bn < NDIRECT)
  {
    if ((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if (bn < NINDIRECT)
  {
    // Load singly-indirect block, allocating if necessary.
    if ((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0)
    {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  
    // Load doubly-indirect block, allocating if necessary.
    bn -= NINDIRECT;
    //allocate
    if ((addr = ip->addrs[DDIRECT]) == 0)
    {
      ip->addrs[DDIRECT] = addr = balloc(ip->dev);
    }
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
     //doubly-indirect block ,we need to allocate on the block 
    int index = bn / NINDIRECT;
    for (int i = 0; i <= index; i++)
    {
      if ((addr = a[i]) == 0)
      {	
        a[i] = addr = balloc(ip->dev);
        //log write must be in the loop 
        //because when we read we have not the fs syscall
        log_write(bp);
      }
    }
  	
    brelse(bp);
     //read the exacyly indirect block , and allocate a block and write 
    bn -= NINDIRECT * index;
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    if ((addr = a[bn]) == 0)
    {
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  

  panic("bmap: out of range");
}
```

Make sure that `struct inode` and `struct dinode` have the same number of elements in their `addrs[]` arrays. We need  to fix the `struct dinode`  in the same way.

`itrunc`  function need to erase the content on the all block belonging to the `inode` or `file`

```c
// Truncate inode (discard contents).
// Caller must hold ip->lock.
void itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for (i = 0; i < NDIRECT; i++)
  {
    if (ip->addrs[i])
    {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if (ip->addrs[NDIRECT])
  {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    for (j = 0; j < NINDIRECT; j++)
    {
      if (a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }
  if (ip->addrs[DDIRECT])
  {
    bp = bread(ip->dev, ip->addrs[DDIRECT]);
    a = (uint *)bp->data;
    struct buf *b;
    uint * ba;
    for (i = 0; i < NINDIRECT; i++)
    {
      if (a[i])
      {
        b = bread(ip->dev, a[i]);
        ba = (uint *)b->data;
        for (j = 0; j < NINDIRECT; j++){
          if(ba[j]) bfree(ip->dev,ba[j]);
        }
        brelse(b);
        bfree(ip->dev,a[i]);
        a[i] = 0;
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[DDIRECT]);
    ip->addrs[DDIRECT] = 0;  
  }

  ip->size = 0;
  iupdate(ip);
}
```



# Part 2  Symbolic links

In this part of lab , we need to add a system call into `xv6` , which is `symbolic links ` i.e. `soft link` . 

The `symbolic links` wants we to creates a new `file` which type is still `FD_INODE`, but the  `inode's` type is  `T_SYMBOLIC` ,  so we in the system call we need to allocate a new `inode` ,and allocate a new block for the first `direct block` .and write the length of the path and the path string in the block. Because the size of the length must a four-byte variable ,which is convenient for the `read` data from the block.

```c

// labfs symbolic system call
uint64
sys_symlink(void)
{ 
  char  target[MAXPATH], path[MAXPATH];
 // struct inode *dp;
  struct inode *ip;
  //struct buf * b;
  //char * c;
  if (argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
    return -1;
  begin_op();
  //create will have the lock for the inode ip
  if((ip =  create(path,T_SYMLINK,0,0))==0){
    end_op();
    return -1;
  }
  int len = strlen(target);
  writei(ip,0,(uint64)&len,0,sizeof(len));
  writei(ip,0,(uint64)target,sizeof(len),len+1);
  iunlockput(ip);
  end_op();
  return 0;
}

```

In  `sys_open` system call , we need to fix the  function to handle the `inode's` type is `T_SYMBOLIC`. And we should  return false when we in a circle!

```c
int depth = 0,len ;
  char next[MAXPATH+1];
  if(!(omode&O_NOFOLLOW)){
    for( ; depth<10 && ip->type ==T_SYMLINK ;depth++){
      readi(ip,0,(uint64)&len,0,sizeof(len));
      readi(ip,0,(uint64)next,sizeof(len),len);
      next[len]= 0;
      iunlockput(ip);
      if((ip=namei(next))==0){
        end_op();
        return -1;
      }
      ilock(ip);
    }
  }
  if(depth>=10){
    iunlockput(ip);
    end_op();
    return -1;
  }
```

# Link

[Link to Lab Page](https://pdos.csail.mit.edu/6.828/2020/labs/fs.html)