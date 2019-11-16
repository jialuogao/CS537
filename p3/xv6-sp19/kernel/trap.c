#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  
  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }
  
  uint stackbot;
  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpu->id == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu->id, tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
    stackbot = (uint)PGROUNDDOWN(proc->stacksz);
    uint cr2 = rcr2();
    //cprintf("stackbot: %d, stacksz: %d, esp %d, proc ebp%d, cr2 %d\n", stackbot, proc->stacksz, tf->esp, proc->tf->ebp, cr2);
    if(cr2 < PGSIZE || (cr2 >= proc->mapcount * PGSIZE + PGSIZE && cr2 < HEAPBOT)) { 
      //cprintf("Error: unmapped shared page is reached\n"); 
      goto kill; 
    }
    else if(cr2 >= proc->sz && cr2 < stackbot - PGSIZE){ 
      //cprintf("Error: PGFLT more than one pg below stack\n"); 
      goto kill; 
    }
    else if(cr2 < stackbot && cr2 >= HEAPBOT){
       //cprintf("Growing stack...\n");
       uint newpg = stackbot - PGSIZE;
       uint heaptop = (uint)PGROUNDUP(proc->sz);
       if(newpg - heaptop < 5 * PGSIZE){ 
         //cprintf("Error: less than 5 page remaining\n"); 
	 goto kill; 
       }
       allocuvm(proc->pgdir, newpg, newpg + PGSIZE);
       proc->stacksz = newpg;
       break;
    }
    //else cprintf("Error exec never should be there!\n");
    //if(0){
    kill: 
       //cprintf("this process has been killed!\n");
       proc->killed = 1;
    //}
    break;
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpu->id, tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running 
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
