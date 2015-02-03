#include "kernel.h"
#define PCB_SIZE 16
#define NBUF 5
#define NR_PROD 3
#define NR_CONS 4
extern PCB idle;
PCB pcb_struct[PCB_SIZE];
int pcb_count = 0;
ListHead ready, block, free;

PCB* fetch_pcb(pid_t pid){
	return &pcb_struct[pid];
}
PCB* create_kthread(void *fun) {
	assert(pcb_count != PCB_SIZE);
	PCB *pcb = fetch_pcb(pcb_count);
	pcb->pid = pcb_count;
	pcb_count ++;
	init_pcb(pcb->pid);
	((TrapFrame*)pcb->tf)->eip = (uint32_t)fun;
	pcb->cr3 = get_kcr3();

	return  pcb;
}	

void
init_proc() {
	PCB *pcb = &idle;
	pcb->pid = 1000;
	pcb->cr3 = get_kcr3();
	list_init(&ready);
	list_init(&block);
	list_init(&(current->list));
	list_add_before( &ready, &(current->list) );
}
/* FOR LOCK & SEMAPHORE --------------------------------------------------------------------------
			--------------------------------------------------------------------------
			--------------------------------------------------------------------------*/
void sleep(ListHead *block, PCB *pcb){
	lock();
	list_del(&pcb->list );
	list_add_before(block, &pcb->list);
	asm volatile("int $0x80");//这里触发来个trap，在中断处理函数中对这个trap不做任何处理，直接跳过
	unlock();
}
void wakeup(PCB *p){
	lock();
	list_del( &(p->list) );
	list_add_before( &ready, &(p->list) );
	unlock();
}
//需要解决使用关中断作为锁的三大问题：1，在V操作之前如果已经lock，那么PV操作会使它unlock
//2，不要在lock之后sleep
//3,不要在中断处理函数中使用PV操作，PV操作中的unlock会使中断嵌套.
void lock(){
	if( current->lock_count == 0 && ( ~read_eflags()&IF_MASK ) ){
		/* here is why if_flag is for, it is to denote whether a critical
		   section is in a irq_handle code, if so, it's not permitted to 
		   sti() in unlock function, just increse and decrease the lock count */
		current->if_flag = false;
	}else{
		cli();
	}
	current->lock_count ++;
}

void unlock(){
	if( current->lock_count == 1 ){
		if( current->if_flag == true ){
			current->lock_count --;
			sti();
		}else{
		/* it is when the process is in irq_handle, which means you cannot
		   sti() */
			current->lock_count--;
			current->if_flag = true;
		}
	}else{
		current->lock_count --;
	}
}

void P(Sem *s){
	lock();
	if( s->token != 0 ){
		s->token--;
	}else{
		/* counter per thread, so it will not affect other threads.
		   inside sleep(), soft interruption make thread switch happen 
		   when switching happens, irq_handle will recover the right
		   lock state of the thread switched to. */
		sleep(&s->block, current);
	}
	unlock();
}
void V(Sem *s){
	lock();
	ListHead *p = (s->block.next);
	if( p == &(s->block) ){
		s->token ++;
	}else{
		wakeup(list_entry(s->block.next, PCB, list));
	}
	unlock();
}
void create_sem( Sem* s, int count ){
	list_init( &s->block );
	s->token = count; 
}
/* END OF LOCK--------------------------------------------------------------------------
			--------------------------------------------------------------------------
			--------------------------------------------------------------------------*/

//消息机制的send和receive
void send(pid_t dest, Msg *m){
	/* different thread share the same address space, so can easily get the PCB */
	PCB *pcb = fetch_pcb(dest); 
	Sem *msg_full = &(pcb->msg_full); 
	/* Don't use P(mutex), send will be used in irq_handle. Using P() will
	   cause nested interruption. */
	lock();
	/* Access resource in a critical region : the other PCB can access too */
	/* ------------------------------------------------------------------- */
	if(list_empty(&pcb->msg_free)){
		/* this is when the target thread has no free message buffer */
		if ( m->src == -2 ) printk("yes!\n");
		printk("the requested pid---%d\n, the requesting pid --- %d \n,type---%d\n",dest, m->src,m->type);
		assert(0);
	}
	ListHead *list = pcb->msg_free.next;
	list_del(list );
	list_add_before(&pcb->msg,list );
	Msg *msg = list_entry(list, Msg, list);	
	memcpy(msg, m, sizeof(Msg) - sizeof(ListHead));
	unlock();
	/* Because V() don't sleep() itself, which can be used in irq_handle */
	V( msg_full ); 
}
void receive(pid_t src, Msg *m){
	ListHead *msg_list = &(current->msg);
	Sem *msg_full = &(current->msg_full);
	while(true){
		P( msg_full );
		lock();
		ListHead *index_list = msg_list->next;
		while( index_list != msg_list ){
			Msg* msg = list_entry(index_list, Msg, list);
			if( src == ANY || msg->src == src ){
				memcpy(m, msg, sizeof(Msg) - sizeof(ListHead));
				list_del( index_list );
				list_add_before( &current->msg_free, index_list );
				unlock();
				return;
			}
			index_list = index_list->next;
		}
		unlock();
		V( msg_full );
	}
}

void init_pcb(pid_t req_pid){
	/* initialize the pcb */
	PCB *pcb = fetch_pcb(req_pid);
	CR3 *cr3 = pcb->cr3;
	if(pcb->list.prev != NULL)
		list_del(&pcb->list);
	memset(pcb, 0, sizeof(PCB));
	pcb->pid = req_pid;
	pcb->cr3 = cr3;
	/* append the argument */	
	TrapFrame *t = (TrapFrame*)(pcb->kstack + KSTACK_SIZE - sizeof(TrapFrame));
	pcb->tf = t;
	// t->ss = (uint32_t)args;
	list_init(&(pcb->msg));
	create_sem(&(pcb->msg_mutex), 1);
	create_sem(&(pcb->msg_full), 0);
	pcb->lock_count = 0;
	pcb->if_flag = true;
	list_init( &pcb->msg_free );
	int i;
	for( i=0; i < MSG_BUFF_SIZE; i++){
		list_add_before( &pcb->msg_free, &pcb->msg_buff[i].list);
	}	

	t->cs = 8;
	t->eflags = 0x206;
	list_init(&pcb->list);
}