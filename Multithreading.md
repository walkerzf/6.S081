---
title: 6.S081 Multithreading Lab
date: 2020-11-17 14:10:00
categories:
  - 6.S081
tags:
  - Lab
toc: true # 是否启用内容索引
---
# MultithreadingLab

his lab will familiarize you with multithreading. You will implement switching between threads in a user-level threads package, use multiple threads to speed up a program, and implement a barrier.

# Uthread: switching between threads

The `Thread 0` is the main function , which will not be scheduled again after scheduled out .

Similar to the `sched` and `schedule ` function , in `uthread.c` ,the `ra` and `sp`  and  other registers will be store  and restore other thread’s registers in `thread_schedule`  function (which is coded by asm ),so once the creation of the thread , the `ra` will  be corresponding function , `sp` will the **top** of their own  `stack` .

Once every thread is created, ` thread_schedule`, the three thread will continue to switching!

In `` thread_schedule`` ,store and restore!

```assembly
thread_switch:
	/* YOUR CODE HERE */
	 	sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
	ret    /* return to ra */
```

```c
thread_switch((uint64)t,(uint64)current_thread);
```

Creation of thread

```c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  t->ra = (uint64)func;
  t->sp = (uint64)(t->stack+STACK_SIZE);
}
```

This is different from the thread switching in the kernel . In this case ,we  switch between different threads directly. In kernel , we switch to the CPU’s schedule  thread ,and pick a thread to switch to.

# Using threads

We will create a lock for each bucket!

```c
pthread_mutex_t lock[NBUCKET]; 
struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];
```

For concurrent `put` , we need to acquire lock for each bucket!

```c

static 
void put(int key, int value)
{
  int i = key % NBUCKET;
  pthread_mutex_lock(&lock[i]); 
  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);
  } 
  pthread_mutex_unlock(&lock[i]); 
}
```

# Barriers

We need all threads reach the `barrier` , we go the next stage. So we need to sleep a thread when it reaches. When all threads reach the `barrier` , clear the ` cnt` 、 increase the `round` and `wakeup` other sleep thread!

```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  if(bstate.nthread==nthread){
      bstate.round++;
      bstate.nthread = 0;
      pthread_cond_broadcast(&bstate.barrier_cond); 
  }else{
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
  
}
```



# Link

[Thread Lab](https://pdos.csail.mit.edu/6.828/2020/labs/thread.html)