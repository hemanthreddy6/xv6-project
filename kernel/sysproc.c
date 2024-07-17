#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  // Instead of allocating memory, only the size is incremented.
  // When these memory addresses are accessed, it leads to a page fault, and only then is the memory allocated.
  myproc()->sz+=n;
  if(n<0)
  {
    uvmdealloc(myproc()->pagetable, addr, myproc()->sz);
  }
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Function to traverse the pagetable and prints the pagetable entries.
void
pagetable_print(pagetable_t pt,uint64 va)
{
  for(int i=0;i<512;i++)
  {
    if(pt[i]&PTE_V)
    {
      if(pt[i]&(PTE_R|PTE_W|PTE_X))
      {
        if(pt[i]&PTE_U)
        {
          printf("PTE No: %d, Virtual Page Address: %p, Physical Page Address %p\n",i,(((va<<9L)+i)<<12L),PTE2PA(pt[i]));
        }
      }
      else
      {
        pagetable_print((pagetable_t)PTE2PA(pt[i]),(va<<9L)+i);
      }
    }
  }
}

// [Task-1] prints the valid pagetable entires of the current process.
uint64
sys_pgtPrint(void)
{
  pagetable_t pt=myproc()->pagetable;
  pagetable_print(pt,0L);
  return 0;
}

// Function to get the data for access and dirty bits for 'n' pages starting from 'va' into 'addr'.
uint64 mypageaccess(uint64 va, int n, uint64 addr)
{
  pagetable_t pt=myproc()->pagetable;
  uint64 bitset=0L;
  for(int i=va,x=0;x<n;i+=PGSIZE,x++)
  {
    pte_t* pte=walk(pt,i,0);
    if(*pte&PTE_V)
    {
      if(*pte&PTE_A)
      {
        bitset|=(1L<<(2*x+1));
      }
      if(*pte&PTE_D)
      {
        bitset|=(1L<<(2*x));
      }
    }
    *pte|=PTE_A;
    *pte-=PTE_A;
  }
  
  if(copyout(pt,addr,(char*)&bitset,sizeof(bitset))!=0)
  {
    return -1;
  }
  return 0;
}

// [Task-3] Copies the bitmap of access and dirty data for 'n' pages starting from 'va' into 'addr'.
uint64
sys_pgaccess(void)
{
  uint64 va,addr;
  int n;
  argaddr(0,&va);
  argint(1,&n);
  argaddr(2,&addr);
  if(n>32)
  {
    panic("pgaccess: too many pages");
    return -2;
  }
  if(mypageaccess(va,n,addr)<0)
  {
    return -1;
  }
  return 0;
}