// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "kalloc.h"

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
//-------malloc and free-------
struct node{
  struct node*next;
  uint64 siz;
  uint64 tag;//0-1
};
#define HEAD sizeof(struct node)
struct{
  struct spinlock lock;
  struct node *freelist;
} malloc_mem;

void check(){
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    while(p->next&&(char*)p+(p->siz)==(char*)p->next&&p->tag&&p->next->tag){
      struct node *nx=p->next;
      p->siz+=nx->siz;
      p->next=nx->next;
    }
   // printf("%p %d\n",p,p->siz);
  }
  release(&malloc_mem.lock);
}
void debug(){
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    printf("%p %d %d\n",p,p->siz,p->tag);
  }
  release(&malloc_mem.lock);
}
void new_free(void* pa){
  //printf("%s\n",pa);
  struct node *r=0;
  if((uint64)pa < now_PHYSTOP || (uint64)pa >= MALLOCSTOP){
    panic("free1");
  }
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    //printf("%p %p\n",(char*)p+HEAD,pa);
    if((char*)p+HEAD==pa){
      if(p->tag==0){
        r=p;
      }
      else{
        panic("free2");
      }
    }
  }
  if(!r)panic("free3");
  //memset(r+HEAD,0,r->siz-HEAD);
  r->tag=1;
  release(&malloc_mem.lock);
  /*acquire(&malloc_mem.lock);
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
  release(&malloc_mem.lock);*/
  //debug();
  check();
  //debug();
}
void malloc_freerange(void *pa_start, void *pa_end){
 // printf("%p\n",pa_end);
  memset((char*)pa_start,0,(char*)pa_end-(char*)pa_start);
  acquire(&malloc_mem.lock);
  malloc_mem.freelist=(struct node*)pa_start;
  malloc_mem.freelist->next=0;
  malloc_mem.freelist->siz=(char*)pa_end-(char*)pa_start;
  malloc_mem.freelist->tag=1;
  release(&malloc_mem.lock);
}
void * new_malloc(uint64 siz){
  struct node *r=0;
  acquire(&malloc_mem.lock);
  for(struct node *p=malloc_mem.freelist;p;p=p->next){
    if(p->tag==1&&p->siz-HEAD>=HEAD+siz){
        //printf("%p %p %p\n",p,(p->siz),(char*)p+(p->siz));
        r=(struct node*)((char*)p+((p->siz)-siz-HEAD));
        //for(int i=0;i<=16;i++)
        //printf("%p %p\n",&(r->siz),MALLOCSTOP);
        (r->siz)=(HEAD+siz);
        memset((char*)r+HEAD,0,r->siz-HEAD);
        //printf("%d\n",r->siz);
        //printf("%p\n",r);
        r->tag=0;  
        r->next=p->next;

        p->siz-=HEAD+siz;
        p->next=r;
        //printf("%p\n",p);

        break;
    }
  }
  release(&malloc_mem.lock);
 // printf("%p\n",r+HEAD);
 //debug();
  return (void*)((char*)r+HEAD);
}
void malloc_init(){
  initlock(&malloc_mem.lock,"malloc_mem");
  malloc_freerange((void*)now_PHYSTOP,(void*)MALLOCSTOP);
 // printf("malloc init is ok");
}

#define LARGE_SIZE 4096
#define MAX_ALLOC_SIZE 1024
//用于测试内存是否可以正确地写入和验证数据
void test_memory(char *ptr, int size, char pattern) {
//    printf("%p\n",ptr);
    for(int i = 0; i < size; i++) {
        ptr[i] = pattern;
        //printf("%d\n",i);
    }
    char *np=new_malloc(100);//覆盖测试
    //printf("%p\n",np);
    for(int i = 0; i < size; i++) {
        if(ptr[i] != pattern) {
            printf("Memory check failed at position %d\n", i);
            exit(0);
        }
    }
    new_free(np);
}
void test_siz(int siz){
  char *ptr = new_malloc(siz);
  if(ptr == 0) {
      printf("test_malloc failed to allocate memory\n");
      exit(0);
  }
  test_memory(ptr, siz, 0xAA);
  new_free(ptr);
  printf("test_siz:%d passed\n",siz);
}
uint64 test_malloc() {
    // 基本的申请和释放测试p
    for(int siz=1;siz<=(8<<20);siz<<=1){
      test_siz(siz);
    }
    test_siz((15<<20));
    // 其他测试添加在此处
    printf("All tests passed\n");
    return 0;
}