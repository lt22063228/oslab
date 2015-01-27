#include "kernel.h"
#include "hal.h"
//duplicate define below
#define PCB_SIZE 16
pid_t PM;
extern pid_t MM;
extern pid_t FM;
extern PCB pcb_struct[PCB_SIZE];
extern int pcb_count;
uint8_t header[512];
void create_process(void);
PCB* new_pcb(void);
void create_sem( Sem* s, int count );
void init_pm(void){
//	create_process();	
}
void create_process(void){
	//获取CR3以及一下pcb的初始化
	PCB *p = new_pcb();			
	Msg m;
	m.src = current->pid;
	m.req_pid = p->pid;	
	m.type = NEW_PCB;
	send( MM, &m);
	receive( MM, &m );
	p->cr3 = (CR3*)m.ret;
	
	//映射内核地址空间
	m.src = current->pid;
	m.dest = MM;
	m.req_pid = p->pid;
	m.type = MAP_KERNEL;
	send( MM, &m );
	receive( MM, &m );

	//获取0号文件的前512字节，这里面包含来ELF HEADER 以及 PROGRAM HEADER
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
		/* what we get is ----------------- pa */
		m.src = current->pid;
		m.type = NEW_PAGE;
		m.dest = MM;
		m.req_pid = p->pid;
		m.offset = ph->vaddr;
		m.len = ph->memsz;
		send( MM, &m );
		receive( MM, &m );
		va = pa_to_va((void*)m.ret);

		/* read ph->filesz bytes starting from offset ph->off from file "0" into pa */
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
	PCB *p = &pcb_struct[pcb_count];
	p->pid = pcb_count;
	pcb_count ++;
	TrapFrame *t = (TrapFrame*)(p->kstack+KSTACK_SIZE-sizeof(TrapFrame));
	p->tf = t;
	list_init( &(p->msg) );
	create_sem( &(p->msg_mutex), 1 );
	create_sem( &(p->msg_full), 0 );

	//消息机制中用于缓冲消息的缓冲区
	p->lock_count = 0;
	p->if_flag = true;
	list_init( &p->msg_free );
	int i;
	for( i=0; i < MSG_BUFF_SIZE; i++){
		list_add_before( &p->msg_free, &p->msg_buff[i].list);
	}
	t->cs = 8;
	t->eflags = 0x206;
	list_init(&p->list);
	/*
	lock();
	list_add_before( &ready, &(p->list));
	unlock();
	*/

//这两个域要通过MM和FM获取
//	p->cr3 = get_kcr3();
//	t->eip = (uint32_t)fun;
	return p;
}

