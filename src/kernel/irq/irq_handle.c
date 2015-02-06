#include "kernel.h"

#define NR_IRQ_HANDLE 32

/* In Nanos, there are no more than 16(actually, 3) hardward interrupts. */
#define NR_HARD_INTR 16

/* Structures below is a linked list of function pointers indicating the
   work to be done for each interrupt. Routines will be called one by
   another when interrupt comes.
   For example, the timer interrupts are used for alarm clocks, so
   device driver of timer will use add_irq_handle to attach a simple
   alarm checker. Also, the IDE driver needs to write back dirty cache
   slots periodically. So the IDE driver also calls add_irq_handle
   to register a handler. */

struct IRQ_t {
	void (*routine)(void);
	struct IRQ_t *next;
};

static struct IRQ_t handle_pool[NR_IRQ_HANDLE];
static struct IRQ_t *handles[NR_HARD_INTR];
static int handle_count = 0;

void
add_irq_handle(int irq, void (*func)(void) ) {
	struct IRQ_t *ptr;
	assert(irq < NR_HARD_INTR);
	if (handle_count > NR_IRQ_HANDLE) {
		panic("Too many irq registrations!");
	}
	ptr = &handle_pool[handle_count ++]; /* get a free handler */
	ptr->routine = func;
	ptr->next = handles[irq]; /* insert into the linked list */
	handles[irq] = ptr;
}
void schedule();
// void do_syscall(TrapFrame *tf);
uint32_t count = 0;
void irq_handle(TrapFrame *tf) {
	int irq = tf->irq;
	count ++;
	// if(current->pid < 10){
	// 	printk("kernel. irq: %d\n",irq);
	// }
	if (irq < 0) {
		panic("Unhandled exception!");
	}
	// if(current->pid == 11){
	// 	printk("syscall id is %d\n", tf->eax);
	// 	printk("hohoho\n");
	// }
	// if(irq == 13){
	// 	uint32_t error_code = tf->error_code;
	// 	printk("13 fault, pid is:%d, error code:%d\n",current->pid, error_code);
	// }else if(irq == 14){
	// 	/* page fault */	
	// 	panic("page fault! pid:%d\n",current->pid);
	// }
	if (irq == 128) {
		/* system call */
		// if(current->pid == 10){
		// 	printk("achor\n");
		// }
		int id = tf->eax;
		if(id == 4){
			printk("syscall.c: pid:%d, fork.\n", current->pid);
			/* tf is parent process's trapframe, first level*/
			/* msg.src contains parent pid, which can be used to 
			   fetch_pcb(),  but that is not enough, we need 'tf'
			   to store the first level trapframe pointer */
			Msg msg;
			extern pid_t PM;
			msg.src = current->pid;
			msg.type = FORK;
			/* here, tf is the first level trapframe */
			msg.buf = (void*)tf;
			send(PM, &msg);
			PDE *pde = (PDE*)(current->cr3->page_directory_base << 12);
			PTE *pte = (PTE*)(pde->page_frame << 12);

			printk("ptable + 796:%d\n", (pte+796)->present);
			receive(PM, &msg);
			/* parent process return child's pid */
			tf->eax = msg.ret;
		}
		// do_syscall(tf);	
	}else if (irq < 1000) {
		extern uint8_t logo[];
		panic("From PID:%d\nCOUNT:%d\nUnexpected exception #%d\n\33[1;31mHint: The machine is always right! For more details about exception #%d, see\n%s\n\33[0m", current->pid, count, irq, irq, logo);
	} else if (irq >= 1000) {
		/* The following code is to handle external interrupts.
		 * You will use this code in Lab2.  */
		int irq_id = irq - 1000;
		assert(irq_id < NR_HARD_INTR);
		struct IRQ_t *f = handles[irq_id];

		while (f != NULL) { /* call handlers one by one */
			f->routine(); 
			f = f->next;
		}
	}
	/* tf is the updated trapFrame pointer, original data in current->tf is the pointer when
	 * current process is awaken last time, it needs being updated to tf. */
	if(current->pid == 10){
		printk("irq_handle:71\n");
	}
	current->tf = tf;
	schedule();
}

