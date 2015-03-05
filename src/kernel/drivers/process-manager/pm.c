#include "kernel.h"
#include "hal.h"
//duplicate define below
#define PCB_SIZE 16
#define KSTACK_SIZE 4096
#define NR_PDE 1024
#define NR_PTE 1024
#define NR_WAITER 1024
pid_t PM;
extern pid_t MM;
extern pid_t FM;
extern int pcb_count;
uint8_t header[512];
static void create_process(int file_idx);
static PCB* new_pcb(void);
static void pmd(void);
static void load_header(void* header, int file_idx);
static void map_kernel(pid_t req_pid);
static void load_segment(pid_t req_pid, struct ELFHeader *elf, int file_idx);
static void fork(Msg *msg);
static void exec(Msg *msg);
static void exit(Msg *msg);
static void wait_pid(Msg *msg);
static void allocate_stack(pid_t req_pid);

typedef struct Waiter{
	pid_t req_pid;
	pid_t src;
	ListHead list;
} Waiter;
Waiter waiters[NR_WAITER];
ListHead waiter_free, waiter_used;

void init_pm(void){
	int i;
	list_init(&waiter_free);
	for(i = 0; i < NR_WAITER; i++){
		ListHead *list = &waiters[i].list;
		list_init(list);
		list_add_before(&waiter_free, list);
	}
	list_init(&waiter_used);
	PCB *pcb = create_kthread(pmd);
	PM = pcb->pid;
	wakeup(pcb);
}
static void pmd(void){
	create_process(0);
	// create_process(5);
	static Msg msg;
	while(1){
		receive(ANY, &msg);
		switch(msg.type){
		case FORK:
			fork(&msg);	
			break;
		case EXEC:
			exec(&msg);	
			break;
		case EXIT:
			exit(&msg);
			break;	
		case WAITPID:
			wait_pid(&msg);
			break;
		default: 
			assert(0);
			break;
		}
	}
}
static void exec(Msg *msg){
	/* input : source pid, filename, virtual address of argument */
	/* output: new process running, responsive message sending */
	pid_t req_pid = msg->src;
	// static char args[256] = {0};
	PCB *pcb = fetch_pcb(req_pid);
	int file_idx = msg->dev_id;	

	/* copy arguments into kernel */
	// void *addr = msg->buf;
	// strcpy_to_kernel(pcb, args, addr);

	/* reclaim the resource */
	/* resource include : lock, message, semaphore */
	/* as a user process, it's now blocked in a receive */
	/* all locking, messaging reserve */
	/* so nothing to reclaim right now */

	/* reinitialize address spcae */
	/* first, invalidate address space*/
	msg->src = current->pid;
	msg->type = EXEC;
	msg->req_pid = req_pid;
	send(MM, msg);
	receive(MM, msg);

	/* initialize the pcb */
	init_pcb(req_pid);

	allocate_stack(req_pid);

	/* map the kernel address */
	map_kernel(req_pid);	

	/*get top 512 bytes from file-0, including ELF HEADER and PROGRAM HEADER */
	struct ELFHeader *elf = (struct ELFHeader *)header;
	load_header(header, file_idx);	

	/*load each program segment*/
	load_segment(req_pid, elf, file_idx);	

	/* initialize the PCB, kernel stack, put the user process into ready queue */
	((TrapFrame*)pcb->tf)->eip = (uint32_t)((struct ELFHeader*)elf)->entry;
	wakeup(pcb);
}

static void fork(Msg *msg){
	/*fork the process from source pid */	
	// msg.src = 100;	
	msg->dest = 100;
	PCB *parent = fetch_pcb(msg->src);
	PCB *child = new_pcb();

	/*update pointer field pointing to kernel stack, ie, tf & ebp*/
	/*calculate offset */
	int kstack_offset = (int)child - (int)parent;

	/*update tf*/
	child->tf = msg->buf + kstack_offset;
	/*copy the kernel stack */
	memcpy(child->kstack, parent->kstack, KSTACK_SIZE);
	/*after memcpy, we can set the return value */
	((TrapFrame*)(child->tf))->eax = 0;
	/*update ebp*/
	TrapFrame *tf = child->tf;
	TrapFrame *end = (TrapFrame*)(child->kstack+KSTACK_SIZE-sizeof(TrapFrame));
	for(; tf < end; tf = (TrapFrame*)tf->ebp){
		tf->ebp = tf->ebp + kstack_offset;	
	}
	uint32_t *pointer = ((uint32_t*)(child->tf));
	while(pointer < (uint32_t*)(child->kstack + KSTACK_SIZE)){
		if(*pointer > (uint32_t)parent->kstack && *pointer < (uint32_t)(parent->kstack + KSTACK_SIZE)){
			*pointer += kstack_offset;	
		}
		pointer ++;
	}

	/*copy all the content of the parent's address space
	  from cr3 register .*/
	/*notice that content of stack is not included, since 
	  we use kernel stack, do it above ^.^ */

	/*allocate the CR3 */
	msg->src = current->pid;
	msg->req_pid = child->pid;
	msg->type = NEW_PCB;
	send(MM, msg);	
	receive(MM, msg);
	child->cr3 = (CR3*)msg->ret;

	/*fork CR3's content, deep copy*/
	msg->src = current->pid;	
	msg->type = FORK;
	msg->buf = parent->cr3;
	msg->req_pid = child->pid;
	send(MM, msg);
	receive(MM, msg);
	wakeup(child);
	msg->src = current->pid;
	msg->ret = child->pid;
	send(parent->pid, msg);
}

static void create_process(int file_idx){
	/* get CR3 and initialize PCB */	
	PCB *pcb = new_pcb();			
	Msg msg;
	pid_t req_pid = pcb->pid;
	msg.src = current->pid;
	msg.req_pid = pcb->pid;	
	msg.type = NEW_PCB;
	send( MM, &msg);
	receive( MM, &msg );
	/* the only one which is physical address */
	pcb->cr3 = (CR3*)msg.ret;
	
	/* create the stdio */
	int i;
	for(i = 0; i < 3; i ++){
		msg.src = current->pid;
		msg.req_pid = pcb->pid;
		msg.type = FILE_OPEN;
		msg.dev_id = 1;
		send(FM, &msg);
		receive(FM, &msg);
	}
	/* map the kernel address */
	map_kernel(req_pid);

	allocate_stack(req_pid);	

	/*get top 512 bytes from file-0, including ELF HEADER and PROGRAM HEADER */
	struct ELFHeader *elf = (struct ELFHeader *)header;
	load_header(header, file_idx);	
		
	/*load each program segment*/
	load_segment(req_pid, elf, file_idx);	

	/* initialize the PCB, kernel stack, put the user process into ready queue */
	((TrapFrame*)pcb->tf)->eip = (uint32_t)((struct ELFHeader*)elf)->entry;
	wakeup(pcb);
}

PCB* new_pcb(void){
	
	PCB *pcb = fetch_pcb(pcb_count);
	pcb->pid = pcb_count;
	pcb_count ++;
	init_pcb(pcb->pid);

	return pcb;
}

static void load_header(void* header, int file_idx){
	Msg msg;
	msg.src = current->pid;
	msg.req_pid = current->pid; 
	msg.type = FILE_READ;
	msg.buf = header;
	msg.len = 512;
	msg.dev_id = file_idx;
	msg.offset = 0;
	send( FM, &msg);
	receive( FM, &msg);
}

static void map_kernel(pid_t req_pid){
	Msg msg;
	msg.src = current->pid;
	msg.dest = MM;
	msg.req_pid = req_pid;
	msg.type = MAP_KERNEL;
	send( MM, &msg);
	receive( MM, &msg);
}

static void load_segment(pid_t req_pid, struct ELFHeader *elf, int file_idx){
	Msg msg;
	struct ProgramHeader *ph, *eph;
	ph = (struct ProgramHeader *)((char*)elf + elf->phoff);
	eph = ph + elf->phnum;
	for(; ph < eph; ph ++) {

		/* the program headers are already got from previous load of 512 bytes */
		/* allocate pages starting from ph->vaddr with size ph->memsz */
		/* what we get is now----------------- va */
		msg.src = current->pid;
		msg.type = NEW_PAGE;
		msg.dest = MM;
		msg.req_pid = req_pid;
		msg.offset = ph->vaddr;
		msg.len = ph->memsz;
		send( MM, &msg );
		receive( MM, &msg );

		/* read ph->filesz bytes starting from offset ph->off from file "0" into va */
		/* first of all, load the segment. */	
		msg.src = current->pid;
		msg.type = FILE_READ;
		msg.dest = FM;
		msg.req_pid = req_pid;
		msg.dev_id = file_idx;
		msg.buf = (void*)ph->vaddr ; /* file-manager recognize virtual address */
		msg.offset = ph->off;
		msg.len = ph->filesz;
		send( FM, &msg );
		receive( FM, &msg );

		/* make the remaining memory zero */
		// for( i = va + ph->filesz; i < va + ph->memsz; *i ++ = 0);
		set_from_kernel_mine(fetch_pcb(req_pid), ((void*)ph->vaddr + ph->filesz), 0, (ph->memsz - ph->filesz));
	}
}

static void exit(Msg *msg){
	/* input : req_pid(the process to stop) */
	/* output: none */
	pid_t req_pid = msg->req_pid;	
	PCB *pcb = fetch_pcb(req_pid);

	/* reclaim resource */
	/* no dynamic memory allocated */
	msg->src = current->pid;
	msg->req_pid = req_pid;
	msg->type = EXIT;
	send(MM, msg);
	receive(MM, msg);
	list_del(&pcb->list);
	memset(pcb, 0, sizeof(PCB));

	/* scan the waiter list, send message to waiter */
	ListHead *cur = waiter_used.next;
	while(cur != &waiter_used){
		Waiter *waiter = list_entry(cur, Waiter, list);
		if(waiter->req_pid == req_pid){
			ListHead *next = cur->next;
			list_del(cur);
			list_add_before(&waiter_free, cur);
			msg->src = current->pid;

			send(waiter->src, msg);
			printk("informed!!!!!!!!!!\n");
			cur = next;
		}else{
			cur = cur->next;
		}
	}
}

static void wait_pid(Msg *msg){
	/* add the current thread into waiter list */
	pid_t src = msg->src;
	pid_t req_pid = msg->req_pid;
	ListHead *list = waiter_free.next;
	Waiter *waiter = list_entry(list, Waiter, list);
	waiter->src = src;
	waiter->req_pid = req_pid;
	list_del(list);
	list_add_before(&waiter_used, list);
}

static void allocate_stack(pid_t req_pid){
	Msg msg;
	msg.src = current->pid;
	msg.type = NEW_PAGE;
	msg.req_pid = req_pid;
	msg.offset = USER_STACK_OFFSET;
	msg.len = PAGE_SIZE;
	send(MM, &msg);
	receive(MM, &msg);
}