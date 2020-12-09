---
title: 6.S081 Traps Lab
date: 2020-10-28 20:00:00
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# Trap Lab

This lab explores how system calls are implemented using traps.

# Part 1 Assembly

This part is a warm-up exercise  to let you know a little more about Risc-v Assemble.

# Part 2  Backtrace

A helper function in kernel . `BackTrace ` helps us to print  a list of functions calls on the stack. According to the hints ,we do it step by step. Using the `r-tp` to get the  frame pointer of the current stack frame .

Note that the return address lives at a fixed offset (-8) from the frame pointer of a stack frame, and that the saved frame pointer lives at fixed offset (-16) from the frame pointer.  

The operation on the pointer.

Xv6 allocates one page for each stack in the xv6 kernel at PAGE-aligned address.  If the `fp` not satisfied the one PGSIZE , the `fp` will at the bottom of the call stack

```c

// lab4 part2
// backtrace
void backtrace(void)
{
  uint64 * currentfp =  (uint64 *)r_fp();
  uint64 up ;
  uint64 down ; 
  do
  { 
    printf("%p\n", *(currentfp-1));
    currentfp = (uint64*)(*(currentfp-2));
    up =  PGROUNDUP((uint64)currentfp);
    down = PGROUNDDOWN((uint64)currentfp);  
  } while (up-down==PGSIZE);
  
}
```

# Part 3 Alarm 

In this part , we want to add two system call  to xv6 . 

Once in user code we  invoke system call ,  The mode will convert from `user mode` - > `kernel mode ` .`` ecall->usertrap->usertrapret->sret` ,  after exiting the kernel mode ,the `pc` will jump to the  `p->tramframe->epc` which saves the `sepc` .  So we know when to call `handler` function (time interrupt) , after `interrupt ` we need to jump the `handler` function ,which means we  fix the value in `p->tramframe->epc` . 

The `function pointer ` aka the address of the function ,aka the value of `epc`.

In user code , we invoke `sigalarm`. In this system call implementation, we need to save the `interval` and `function pointer` in `proc` structure , an return user  code ,  the `p->tramfram->epc` will be the next of `ecall`. 

When we have a timer interrupt ,we need  the `epc` be the `function pointer` , for resuming the  interrupted user code . Because usually  we return the interrupted user code, this time ,we need to jump to`handler` function ,we need to reserve the `p->tramframe->*`  and change the `p->tramframe->epc` be the `function pointer` . In `handler ` function ,we invoke `sigreturn` system call. In this implementation , we restore the saved registers  when  interrupted ,and jump to the  original `p->tramframe->epc` resume the user code .

``` c
if (which_dev == 2)
  {
    p->pastedticks++;
    if ((p->pastedticks > 0) && (p->pastedticks == p->interval))
    {
      if (p->permisson == 1)
      {

        p->permisson = 0;
        p->epc = p->trapframe->epc;
        p->ra = p->trapframe->ra;
        p->sp = p->trapframe->sp;
        p->gp = p->trapframe->gp;
        p->tp = p->trapframe->tp;
        p->t0 = p->trapframe->t0;
        p->t1 = p->trapframe->t1;
        p->t2 = p->trapframe->t2;
        p->s0 = p->trapframe->s0;
        p->s1 = p->trapframe->s1;
        p->a0 = p->trapframe->a0;
        p->a1 = p->trapframe->a1;
        p->a2 = p->trapframe->a2;
        p->a3 = p->trapframe->a3;
        p->a4 = p->trapframe->a4;
        p->a5 = p->trapframe->a5;
        p->a6 = p->trapframe->a6;
        p->a7 = p->trapframe->a7;
        p->s2 = p->trapframe->s2;
        p->s3 = p->trapframe->s3;
        p->s4 = p->trapframe->s4;
        p->s5 = p->trapframe->s5;
        p->s6 = p->trapframe->s6;
        p->s7 = p->trapframe->s7;
        p->s8 = p->trapframe->s8;
        p->s9 = p->trapframe->s9;
        p->s10 = p->trapframe->s10;
        p->s11 = p->trapframe->s11;
        p->t3 = p->trapframe->t3;
        p->t4 = p->trapframe->t4;
        p->t5 = p->trapframe->t5;
        p->t6 = p->trapframe->t6;
        p->trapframe->epc = p->handler;
        p->pastedticks = 0;
      } 
    }

    yield();
  }
```

 

```c
// added system call
uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();
  p->permisson = 1;
  p->trapframe->epc = p->epc;
  p->trapframe->ra = p->ra;
  p->trapframe->sp = p->sp;
  p->trapframe->gp = p->gp;
  p->trapframe->tp = p->tp;
  p->trapframe->t0 = p->t0;
  p->trapframe->t1 = p->t1;
  p->trapframe->t2 = p->t2;
  p->trapframe->s0 = p->s0;
  p->trapframe->s1 = p->s1;
  p->trapframe->a0 = p->a0;
  p->trapframe->a1 = p->a1;
  p->trapframe->a2 = p->a2;
  p->trapframe->a3 = p->a3;
  p->trapframe->a4 = p->a4;
  p->trapframe->a5 = p->a5;
  p->trapframe->a6 = p->a6;
  p->trapframe->a7 = p->a7;
  p->trapframe->s2 = p->s2;
  p->trapframe->s3 = p->s3;
  p->trapframe->s4 = p->s4;
  p->trapframe->s5 = p->s5;
  p->trapframe->s6 = p->s6;
  p->trapframe->s7 = p->s7;
  p->trapframe->s8 = p->s8;
  p->trapframe->s9 = p->s9;
  p->trapframe->s10 = p->s10;
  p->trapframe->s11 = p->s11;
  p->trapframe->t3 = p->t3;
  p->trapframe->t4 = p->t4;
  p->trapframe->t5 = p->t5;
  p->trapframe->t6 = p->t6;
  return 0;
}

//added system call
uint64
sys_sigalarm(void)
{
  struct proc *p = myproc();
  
  if (argint(0, &(p->interval)) < 0)
    return -1;
  if (argaddr(1, &(p->handler)) < 0)
    return -1;

  return 0;
}
```

