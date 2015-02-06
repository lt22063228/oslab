#include "kernel.h"
#include "hal.h"
#define SYS_read 1
#define SYS_write 2
#define SYS_puts 3
#define SYS_fork 4
#define SYS_exec 5
#define SYS_exit 6
#define SYS_getpid 7
#define SYS_waitpid 8
#define SYS_gets 9
extern pid_t PM;

static void sys_exit(TrapFrame *tf, Msg *msg);
static void sys_waitpid(TrapFrame *tf, Msg *msg);
static void sys_gets(TrapFrame *tf, Msg *msg);
static void sys_puts(TrapFrame *tf, Msg *msg);

static Msg msg;
void do_syscall(TrapFrame *tf) {
	return;
	int id = tf->eax; // system call id
	switch (id) {
		case SYS_read:
			// ...
			// send(FM, m);
			// receive(FM, m);
			// int nread = m.ret;
			// tf->eax = nread;   // return value is stored in eax
			break;
		case SYS_exit:
			printk("syscall.c: pid:%d, exit.\n", current->pid);
			sys_exit(tf, &msg);
			break;
		case SYS_waitpid:
			printk("syscall.c: pid:%d, waitpid.\n", current->pid);
			sys_waitpid(tf, &msg);
			break;
		case SYS_getpid:
			printk("syscall.c: pid:%d, getpid.\n", current->pid);
			tf->eax = current->pid;
			break;
		case SYS_write:
			/* register eax is to store the return value */
			break;
		case SYS_gets:
			printk("syscall.c: pid:%d, gets.\n", current->pid);
			sys_gets(tf, &msg);
			break;
		case SYS_puts:
			printk("syscall.c: pid:%d, puts.\n", current->pid);
			/* testing puts system call */
			sys_puts(tf, &msg);
			break;
		case SYS_fork:
			printk("syscall.c: pid:%d, fork.\n", current->pid);
			/* tf is parent process's trapframe, first level*/
			/* msg.src contains parent pid, which can be used to 
			   fetch_pcb(),  but that is not enough, we need 'tf'
			   to store the first level trapframe pointer */
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
			break;
		case SYS_exec:
			printk("syscall.c: pid:%d, exec.\n", current->pid);
			/* input : filename, argument string, current pcb */
			/* output: nothing. new process running */
			msg.src = current->pid;
			msg.type = EXEC;		
			/* filename, # in ramdisk */
			msg.dev_id = tf->ebx;
			/* argument address, virtual address w.r.t src proc */
			msg.buf = (void*)tf->ecx;
			send(PM, &msg);
			// receive(PM, &msg);
			sleep(&block, current);
			break;
		default:
			/* kernel thread use system call to self-trap */
			break;
	}
}

static void sys_exit(TrapFrame *tf, Msg *msg){
	/* input : current pcb */
	/* output: reclaim the resource, stop the thread */
	msg->src = current->pid;
	msg->req_pid = current->pid;
	msg->type = EXIT;
	send(PM, msg);
	printk("exit success!!!!\n");
	sleep(&block, current);
}

static void sys_waitpid(TrapFrame *tf, Msg *msg){
	pid_t req_pid = tf->ebx;
	msg->src = current->pid;
	msg->req_pid = req_pid;
	msg->type = WAITPID;
	send(PM, msg);
	printk("wating!!!!!!!!\n");
	receive(PM, msg);
	printk("waiting success!!!!!!!\n");
}

static void sys_gets(TrapFrame *tf, Msg *msg){
	void *buf = (void*)tf->ebx;
	dev_read("tty1", current->pid, buf, 0, 255);	
}
static void sys_puts(TrapFrame *tf, Msg *msg){
	void *buf = (void*)tf->ebx;
	dev_write("tty1", current->pid, buf, 0, 255);	
}
