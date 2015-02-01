#include "kernel.h"
#include "hal.h"
#define SYS_read 1
#define SYS_write 2
#define SYS_puts 3
#define SYS_fork 4
#define SYS_exec 5
extern pid_t PM;
void do_syscall(TrapFrame *tf) {
	int id = tf->eax; // system call id
	int b, c, d; 
	static Msg msg;
	switch (id) {
		case SYS_read:
			// ...
			// send(FM, m);
			// receive(FM, m);
			// int nread = m.ret;
			// tf->eax = nread;   // return value is stored in eax
			break;
		case SYS_write:
			/* register eax is to store the return value */
			break;
		case SYS_puts:
			/* testing puts system call */
			b = tf->ebx;
			c = tf->ecx;
			d = tf->edx;
			printk("a:%d,b:%d,c:%d\n",b,c,d);
			tf->eax = d + b + c;
			break;
			
		case SYS_fork:
			/* tf is parent process's trapframe, first level*/
			/* msg.src contains parent pid, which can be used to 
			   fetch_pcb(),  but that is not enough, we need 'tf'
			   to store the first level trapframe pointer */
			msg.src = current->pid;
			msg.type = FORK;
			/* here, tf is the first level trapframe */
			msg.buf = (void*)tf;
			send(PM, &msg);
			receive(PM, &msg);
			/* parent process return child's pid */
			tf->eax = msg.ret;
			break;
		case SYS_exec:
			/* input : filename, argument string, current pub */
			/* output: nothing. new process running */
			msg.src = current->pid;
			msg.type = EXEC;		
			/* filename, # in ramdisk */
			msg.dev_id = tf->ebx;
			/* argument address, virtual address w.r.t src proc */
			msg.buf = (void*)tf->ecx;
			send(PM, &msg);
			/* when it receive something, current proc become
			   another one */
			receive(PM, &msg);
			break;
		default:
			/* kernel thread use system call to self-trap */
			break;
	}
}