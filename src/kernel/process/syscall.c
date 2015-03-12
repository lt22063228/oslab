#include "kernel.h"
#include "hal.h"
#define SYS_puts 	3
#define SYS_fork 4
#define SYS_exec 5
#define SYS_exit 6
#define SYS_getpid 7
#define SYS_waitpid 8
#define SYS_gets 9
#define SYS_cat 10
#define SYS_open 11
#define SYS_read 12
#define SYS_write 13
#define SYS_close 14
#define SYS_lseek 15
#define SYS_echo 16
#define SYS_makefile 17
#define SYS_listdir 18
#define SYS_mkdir	19
#define SYS_chdir	20
#define SYS_rmdir	21
extern pid_t PM;
extern pid_t FM;

static void sys_exit(TrapFrame *tf, Msg *msg);
static void sys_waitpid(TrapFrame *tf, Msg *msg);
static void sys_gets(TrapFrame *tf, Msg *msg);
static void sys_puts(TrapFrame *tf, Msg *msg);
static void sys_fork(TrapFrame *tf, Msg *msg);
static void sys_exec(TrapFrame *tf, Msg *msg);
static void sys_cat(TrapFrame *tf, Msg *msg);
static void sys_open(TrapFrame *tf, Msg *msg);
static void sys_close(TrapFrame *tf, Msg *msg);
static void sys_read(TrapFrame *tf, Msg *msg);
static void sys_write(TrapFrame *tf, Msg *msg);
static void sys_lseek(TrapFrame *tf, Msg *msg); 
static void sys_echo(TrapFrame *tf, Msg *msg);
static void sys_makefile(TrapFrame *tf, Msg *msg);
static void sys_listdir(TrapFrame *tf, Msg *msg);
static void sys_mkdir(TrapFrame *tf, Msg *msg);
static void sys_chdir(TrapFrame *tf, Msg *msg);
static void sys_rmdir(TrapFrame *tf, Msg *msg);

static Msg msg;
void do_syscall(TrapFrame *tf) {
	int id = tf->eax; // system call id
	if(current->pid == 10){
		printk("haha");
	}
	switch (id) {
		case SYS_read:
			sys_read(tf, &msg);
			break;
		case SYS_write:
			sys_write(tf, &msg);
			break;
		case SYS_open:
			sys_open(tf, &msg);
			break;
		case SYS_close:
			sys_close(tf, &msg);
			break;
		case SYS_lseek:
			sys_lseek(tf, &msg);
			break;
		case SYS_echo:
			sys_echo(tf, &msg);
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
			sys_fork(tf, &msg);
			break;
		case SYS_exec:
			printk("syscall.c: pid:%d, exec.\n", current->pid);
			sys_exec(tf, &msg);
			break;
		case SYS_cat:
			printk("syscall.c: pid:%d, cat.\n", current->pid);
			sys_cat(tf, &msg);
			break;
		case SYS_makefile:
			sys_makefile(tf, &msg);
			break;
		case SYS_listdir:
			sys_listdir(tf, &msg);
			break;
		case SYS_mkdir:
			sys_mkdir(tf, &msg);
			break;
		case SYS_chdir:
			sys_chdir(tf, &msg);
			break;
		case SYS_rmdir:
			sys_rmdir(tf, &msg);
			break;
		default:
			/* kernel thread use system call to self-trap */
			break;
	}
}
static void sys_rmdir(TrapFrame *tf, Msg *msg){
	char *path = (char*)tf->ebx;
	msg->src = current->pid;
	msg->req_pid = current->pid;
	msg->buf = path;	
	msg->type = RMDIR;

	send(FM, msg);
	receive(FM, msg);
}
static void sys_chdir(TrapFrame *tf, Msg *msg){
	char *path = (char*)tf->ebx;
	msg->src = current->pid;
	msg->req_pid = current->pid;
	msg->buf = path;	
	msg->type = CHDIR;

	send(FM, msg);
	receive(FM, msg);
}
static void sys_mkdir(TrapFrame *tf, Msg *msg){
	char *path = (char*)tf->ebx;
	msg->src = current->pid;
	msg->req_pid = current->pid;
	msg->buf = path;	
	msg->type = MKDIR;

	send(FM, msg);
	receive(FM, msg);
}
static void sys_makefile(TrapFrame *tf, Msg *msg){
	char *path = (char*)tf->ebx;
	// int file_type = (int)tf->ecx;
	msg->src = current->pid;
	msg->req_pid = current->pid;
	msg->buf = path;
	msg->type = MAKE_FILE;
	msg->offset = FILE_PLAIN;	

	send(FM, msg);
	receive(FM, msg);
}

// void print_dir(char *buf){
// 	char *source = buf;
// 	char *end = buf + 20*SIZE_OF_BLOCK;
// 	while(*source != '\0' && source < end){
// 		printf("%s\n", source);
// 		source += (strlen(source) + 1);
// 	}
// }
static char str_buf[2048] = {0};
static void sys_listdir(TrapFrame *tf, Msg *msg){
	char *path = "./";
	msg->src = current->pid;
	msg->req_pid = current->pid;
	msg->type = LIST_DIR;
	msg->buf = (void*)str_buf;
	msg->offset = (off_t)path;

	send(FM, msg);
	receive(FM, msg);
	int start = 0, i;
	for(i = 0; i < 2048 && *(str_buf + i) != '\000'; i++){
		if(*(str_buf + i) == '\n'){
			dev_write("tty1", current->pid, str_buf, start, i - start + 1);
			memset(str_buf+start, 0, i - start + 1);
			start = i + 1;
		}
	}
}
static void sys_echo(TrapFrame *tf, Msg *msg){
	char *buf = (char*)tf->ebx;	
	int i = 0;
	while(*(buf + i) != '\000') i++;
	dev_write("tty1", current->pid, buf, 0, i);	
}
static void sys_lseek(TrapFrame *tf, Msg *msg){
	msg->src = current->pid;
	msg->type = FILE_LSEEK;
	msg->req_pid = current->pid;
	msg->dev_id = (int)tf->ebx;
	msg->offset = (int)tf->ecx;
	int whence = (int)tf->edx;
	msg->len = whence;
	send(FM, msg);
	receive(FM, msg);
}
static void sys_write(TrapFrame *tf, Msg *msg){
	msg->src = current->pid;
	msg->type = FILE_WRITE;
	msg->req_pid = current->pid;
	msg->dev_id = (int)tf->ebx;
	msg->buf = (void*)tf->ecx;
	msg->len = (size_t)tf->edx;
	send(FM, msg);
	receive(FM, msg);
}
static void sys_read(TrapFrame *tf, Msg *msg){
	msg->src = current->pid;
	msg->type = FILE_READ;
	msg->req_pid = current->pid;
	msg->dev_id = (int)tf->ebx;
	msg->buf = (void*)tf->ecx;
	msg->len = (size_t)tf->edx;
	send(FM, msg);
	receive(FM, msg);
}
static void sys_close(TrapFrame *tf, Msg *msg){
	msg->src = current->pid;
	msg->type = FILE_CLOSE;
	msg->req_pid = current->pid;
	msg->dev_id = (int)tf->ebx;
	send(FM, msg);
	receive(FM, msg);
}
static void sys_open(TrapFrame *tf, Msg *msg){
	msg->src = current->pid;
	msg->type = FILE_OPEN;
	msg->req_pid = current->pid;
	// msg->dev_id = (int)tf->ebx;
	msg->buf = (char*)tf->ebx;
	send(FM, msg);
	receive(FM, msg);
	tf->eax = msg->ret;
}
static void sys_cat(TrapFrame *tf, Msg *msg){
	/* output file to screen */
	int write_size = 0;
	static char buf[256];
	msg->src = current->pid;
	msg->type = FILE_READ;
	msg->req_pid = current->pid;
	msg->dev_id = (int)tf->ebx;
	msg->buf = buf;
	msg->offset = 0;
	msg->len = 256;
	send(FM, msg);
	receive(FM, msg);
	while(*(buf + write_size)) write_size++;
	dev_write("tty1", current->pid, buf, 0, write_size);	
}
static void sys_exec(TrapFrame *tf, Msg *msg){
	/* input : filename, argument string, current pcb */
	/* output: nothing. new process running */
	msg->src = current->pid;
	msg->type = EXEC;		
	/* filename, # in ramdisk */
	// msg->dev_id = tf->ebx;
	if(msg->dev_id > 4 || msg->dev_id < 0){
		printk("the filename must be an INTEGER!!!!!!!!!!!!!!\n");
		assert(0);
	}
	/* argument address, virtual address w.r.t src proc */
	msg->buf = (void*)tf->ebx;
	send(PM, msg);
	// receive(PM, &msg);
	sleep(&block, current);
}
static void sys_fork(TrapFrame *tf, Msg *msg){
	/* tf is parent process's trapframe, first level*/
	/* msg.src contains parent pid, which can be used to 
	   fetch_pcb(),  but that is not enough, we need 'tf'
	   to store the first level trapframe pointer */
	msg->src = current->pid;
	msg->type = FORK;
	/* here, tf is the first level trapframe */
	msg->buf = (void*)tf;
	send(PM, msg);
	receive(PM, msg);
	/* parent process return child's pid */
	tf->eax = msg->ret;
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
	int n = dev_read("tty1", current->pid, buf, -1, 255);	
	*((char*)buf + n) = 0;
}
static void sys_puts(TrapFrame *tf, Msg *msg){
	void *buf = (void*)tf->ebx;
	dev_write("tty1", current->pid, buf, -1, 255);	
}
