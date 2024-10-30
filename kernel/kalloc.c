// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];//对内存按照CPU数量进行划分

int 
pa2cpuid(void* pa){
  uint64 chunk = (((char*)PHYSTOP - (char*)end)/NCPU);//每个CPU的内存大小
  
  int id=0;
  char *p_start = end;
  char *p_end = end + chunk;
  
  for(;p_end <= (char*)PHYSTOP && id < NCPU ; id++ , p_start += chunk, p_end += chunk){
    if((char*)pa <= p_end && (char*)pa >= p_start)
      return id;
  }

  return -1;
}


void*
find_freepage(int sid){
  struct run *r;
  for(int id = 0 ; id < NCPU ; id++){
    if(id == sid){
      continue;
    }
    //avoid multi-stealing
    acquire(&kmem[id].lock);
    r = kmem[id].freelist;
    if(r){
      //printf("steal from CPU-%d",id);
      kmem[id].freelist = r->next;
      release(&kmem[id].lock);
      return (void*)r;
    }
    release(&kmem[id].lock);
  }
  return (void*)0;
}

struct run*
steal_mem(int sid){
  struct run *r;
  r = (struct run*)find_freepage(sid);
  return r;
}

void
kinit()
{
  //init locks for per-CPU
  uint64 start_offset,end_offset;
  uint64 chunk = (((char*)PHYSTOP - (char*)end)/NCPU);
  for(int id = 0; id<NCPU ; id++){
    initlock(&kmem[id].lock, "kmem");
    start_offset =  id * chunk;
    end_offset = (NCPU-id-1) * chunk;
    freerange((end + start_offset), (void*)(PHYSTOP - end_offset));
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  int tid = pa2cpuid(pa); //target CPU-id

  if(tid < 0)
    panic("runout mem");

  r = (struct run*)pa;
  
  acquire(&kmem[tid].lock);
  r->next = kmem[tid].freelist;
  kmem[tid].freelist = r;
  release(&kmem[tid].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
 
  push_off();
  int cid = cpuid(); // current CPU-id
  pop_off();

  acquire(&kmem[cid].lock);
  r = kmem[cid].freelist;
  if(r){
    kmem[cid].freelist = r->next;
    release(&kmem[cid].lock);
  }
    
  else{
    release(&kmem[cid].lock);
    r = steal_mem(cid);
  } 

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
