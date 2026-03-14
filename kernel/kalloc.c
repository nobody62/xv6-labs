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
} kmem[NCPU];

void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  // char* buf;
  for(int i=0;i<NCPU;++i){
    // snprintf(buf, 5, "kmem%d", i);
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
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

  r = (struct run*)pa;

  push_off();
  int i = cpuid();
  // int i = mycpu();
  pop_off();

  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  release(&kmem[i].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int i = cpuid();
  // int i = mycpu();
  pop_off();

  acquire(&kmem[i].lock);
  r = kmem[i].freelist;
  if(r){
    kmem[i].freelist = r->next;
    release(&kmem[i].lock);
  }
  else{
    release(&kmem[i].lock);
    struct run* rr = 0;  
    for(int j=0;j<NCPU;++j){
      if(j == i)  continue;
      acquire(&kmem[j].lock);
      rr = kmem[j].freelist;
      if(rr){
        // acquire(&kmem[i].lock);
        struct run* newlist = 0;
        for(int k=0;k<10&&rr;++k){
          kmem[j].freelist = rr->next;
          rr->next = newlist;
          newlist = rr;
          rr = kmem[j].freelist;
        }
        release(&kmem[j].lock);

        acquire(&kmem[i].lock);
        if(newlist){
          r = newlist;
          kmem[i].freelist = r->next;
        }
        release(&kmem[i].lock);
        if(r) break;
        }
      else
        release(&kmem[j].lock);    
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
