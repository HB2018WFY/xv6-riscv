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
} kmem;


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)now_PHYSTOP);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= now_PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
struct node{
  struct node*next;
  uint64 siz;
  uint64 tag;//0-1
};
#define HEAD sizeof(node)
struct{
  struct spinlock lock;
  struct node *freelist;
} malloc_mem;

void check(){
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    while(p->next&&(p+p->siz==p->next)&&p->tag&&p->next->tag){
      struct node *nx=p->next;
      p->siz+=nx->siz;
      p->next=nx->next;
    }
  }
  release(&malloc_mem.lock);
}

void new_free(void* pa){
  struct node *r=0;
  if((uint64)pa < now_PHYSTOP || (uint64)pa >= MALLOCSTOP)
    panic("free");
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    if(p==pa){
      if(p->siz==0){
        r=p;
      }
      else{
        panic("free");
      }
    }
  }
  release(&malloc_mem.lock);
  if(!r)panic("free");
  memset(r,0,r->siz);
  r->tag=1;

  acquire(&malloc_mem.lock);
  if(r<malloc_mem.freelist){
    r->next = malloc_mem.freelist;
    malloc_mem.freelist = r;
  } 
  else{
      for(struct node *p=malloc_mem.freelist;p;p=p->next){
        if(p<=r&&(!p->next||p->next>r)){
          r->next=p->next;
          p->next=r;
          break;
        }
    }
  }
  release(&malloc_mem.lock);
  check();
}
void malloc_freerange(void *pa_start, void *pa_end){
  memset((char*)pa_start,0,(char*)pa_end-(char*)pa_start);
  acquire(&malloc_mem.lock);
  malloc_mem.freelist=(struct node*)pa_start;
  malloc_mem.freelist->siz=(char*)pa_end-(char*)pa_start;
  malloc_mem.freelist->tag=1;
  release(&malloc_mem.lock);
}
void * new_malloc(uint64 siz){
  struct node *r=0;
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    if(p->tag==1&&p->siz>=siz){
        r=p+p->siz-siz;
        r->siz=siz;
        r->tag=0;
        r->next=p->next;
        p->siz-=siz;
        p->next=r;
        break;
    }
  }
  release(&malloc_mem.lock);
  return (void*)r;
}
void malloc_init(){
  initlock(&malloc_mem.lock,"malloc_mem");
  malloc_freerange((void*)now_PHYSTOP,(void*)MALLOCSTOP);
}
