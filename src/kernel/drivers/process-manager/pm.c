#include "kernel.h"
#include "hal.h"
//duplicate define below
#define PCB_SIZE 16
#define KSTACK_SIZE 4096
#define NR_PDE 1024
#define NR_PTE 1024
pid_t PM;
extern pid_t MM;
extern pid_t FM;
extern PCB pcb_struct[PCB_SIZE];
extern int pcb_count;
uint8_t header[512];
void create_process(void);
PCB* new_pcb(void);
void create_sem( Sem* s, int count );
static void pmd(void);
void init_pm(void){
	PCB *p = create_kthread(pmd);
	PM = p->pid;
	wakeup(p);
}
static void pmd(void){
	create_process();
	static Msg msg;
	while(1){
		receive(ANY, &msg);
		switch(msg.type){
		case FORK:
			/*fork the process from source pid */	
			// msg.src = 100;	
			msg.dest = 100;
			PCB *parent = fetch_pcb(msg.src);
			PCB *child = new_pcb();

			/*update pointer field pointing to kernel stack, ie, tf & ebp*/
			/*calculate offset */
			int kstack_offset = (int)child - (int)parent;

			/*update tf*/
			child->tf = msg.buf + kstack_offset;
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

			/*copy all the content of the parent's address space
			  from cr3 register .*/
			/*notice that parent's stack is not included, since 
			  we use kernel stack, do it above ^.^ */

			/*allocate the CR3 */
			msg.src = current->pid;
			msg.req_pid = child->pid;
			msg.type = NEW_PCB;
			send(MM, &msg);	
			receive(MM, &msg);
			child->cr3 = (CR3*)msg.ret;

			/*fork CR3's content, deep copy*/
			msg.src = current->pid;	
			msg.type = FORK;
			msg.buf = parent->cr3;
			msg.req_pid = child->pid;
			send(MM, &msg);
			receive(MM, &msg);
			wakeup(child);
			msg.src = current->pid;
			msg.ret = child->pid;
			send(parent->pid, &msg);
			break;
		default: 
			assert(0);
			break;
		}
		volatile int x = 0;
		x ++;

	}
}

void create_process(void){
	/* get CR3 and initialize PCB */	
	PCB *p = new_pcb();			
	Msg m;
	m.src = current->pid;
	m.req_pid = p->pid;	
	m.type = NEW_PCB;
	send( MM, &m);
	receive( MM, &m );
	/* the only one which is physical address */
	p->cr3 = (CR3*)m.ret;
	
	/* map the kernel address */
	m.src = current->pid;
	m.dest = MM;
	m.req_pid = p->pid;
	m.type = MAP_KERNEL;
	send( MM, &m );
	receive( MM, &m );

	/*get top 512 bytes from file-0, including ELF HEADER and PROGRAM HEADER */
	m.src = current->pid;
	m.type = FILE_READ;
	struct ELFHeader *elf = (struct ELFHeader *)header;
	m.buf = header;
	m.len = 512;
	m.dev_id = 0;
	m.offset = 0;
	send( FM, &m );
	receive( FM, &m );
		
	/*load each program segment*/
	struct ProgramHeader *ph, *eph;
	uint8_t *va = 0,*i;
	ph = (struct ProgramHeader *)((char*)elf + elf->phoff);
	eph = ph + elf->phnum;
	for(; ph < eph; ph ++) {

		/* the program headers are already got from previous load of 512 bytes */
		/* allocate pages starting from ph->vaddr with size ph->memsz */
		/* what we get is now----------------- va */
		m.src = current->pid;
		m.type = NEW_PAGE;
		m.dest = MM;
		m.req_pid = p->pid;
		m.offset = ph->vaddr;
		m.len = ph->memsz;
		send( MM, &m );
		receive( MM, &m );
		va = (void*)m.ret;

		/* read ph->filesz bytes starting from offset ph->off from file "0" into va */
		/* first of all, load the segment. */	
		m.src = current->pid;
		m.type = FILE_READ;
		m.dest = FM;
		m.dev_id = 0;
		m.buf =  va ; /* file-manager recognize virtual address */
		m.offset = ph->off;
		m.len = ph->filesz;
		send( FM, &m );
		receive( FM, &m );

		/* make the remaining memory zero */
		for( i = va + ph->filesz; i < va + ph->memsz; *i ++ = 0);
	}

	/* initialize the PCB, kernel stack, put the user process into ready queue */
	((TrapFrame*)p->tf)->eip = (uint32_t)((struct ELFHeader*)elf)->entry;
	wakeup(p);
}
PCB* new_pcb(void){
	/* allocate PCB structure */
	PCB *p = &pcb_struct[pcb_count];
	p->pid = pcb_count;
	pcb_count ++;

	/* create initial TrapFrame, it points to kernel stack*/
	TrapFrame *t = (TrapFrame*)(p->kstack+KSTACK_SIZE-sizeof(TrapFrame));
	p->tf = t;

	/* message and semaphor related data structure */	
	list_init( &(p->msg) );
	create_sem( &(p->msg_mutex), 1 );
	create_sem( &(p->msg_full), 0 );
	p->lock_count = 0;
	p->if_flag = true;
	list_init( &p->msg_free );
	int i;
	for( i=0; i < MSG_BUFF_SIZE; i++){
		list_add_before( &p->msg_free, &p->msg_buff[i].list);
	}

	/* 'cs' and 'eflags' cannot be recover from trapFrame 
	   'eip' either, but cannot get it right now
	   'cr3' is anther important one */	
	t->cs = 8;
	t->eflags = 0x206;
	list_init(&p->list);

	return p;
}

