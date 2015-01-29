#include "kernel.h"
#define PCB_SIZE 16
#define NBUF 5
#define NR_PROD 3
#define NR_CONS 4
void sleep(ListHead *p);
PCB pcb_struct[PCB_SIZE];
int pcb_count = 0;
ListHead ready, block, free;
//测试lock和unlock的变量
//
int buf[NBUF], f=0, r=0, g=1;
int last = 0;
Sem empty, full, mutex;
void create_sem(Sem*, int);
void test_setup(void);

void A();void B();void C();void D();void E();

PCB* fetch_pcb(pid_t pid){
	return &pcb_struct[pid];
}
PCB*
create_kthread(void *fun) {
	/* FOR LAB1 ---------------------------------------------------------------------- */
		PCB* p=0;
		assert(pcb_count != PCB_SIZE);
		p = &pcb_struct[pcb_count++];
	//每个进程拥有一个pid，这里将PID设置为数组index，方便查找
		p->pid = pcb_count-1;
	// the first trapFrame will be in the bottom of the kstack.
		TrapFrame *t = (TrapFrame*)(p->kstack+KSTACK_SIZE-sizeof(TrapFrame));
		p->tf = t;
		p->if_flag = true;
	//	t->ss = (uint32_t)num;//将传给线程的参数存在ss中，调用栈中第一个参数是SS所在的位子。
		t->cs = 8;
		t->eip = (uint32_t)fun;
		t->eflags = 0x206;
		
		list_init(&p->list);
		list_add_before( &ready, &(p->list));
	/* END OF LAB1 -------------------------------------------------------------------- */
		list_init( &(p->msg) );
		create_sem( &(p->msg_mutex), 1 );
		create_sem( &(p->msg_full), 0 );
		//消息机制中用于缓冲消息的缓冲区
		p->lock_count = 0;
		p->cr3 = get_kcr3();
		list_init( &p->msg_free );
		int i;
		for( i=0; i < MSG_BUFF_SIZE; i++){
			list_add_before( &p->msg_free, &p->msg_buff[i].list);
		}

		return  p;
}	

void
init_proc() {
	list_init(&ready);
	list_init(&block);
	list_init(&(current->list));
	list_add_before( &ready, &(current->list) );
//	create_kthread(A);
//	create_kthread(B);
//	create_kthread(C);
//	create_kthread(D);
//	create_kthread(E);
//	test_setup();
//	create_kthread(A,1);
//	create_kthread(A,2);
}

/* FOR LOCK & SEMAPHORE --------------------------------------------------------------------------
			--------------------------------------------------------------------------
			--------------------------------------------------------------------------*/
void sleep(ListHead *block){
	lock();
	list_del( &current->list );
	list_add_before(block, &current->list);
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
		sleep( &s->block );
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
void test_producer(void){
	while(1){
		
		P(&empty);
		P(&mutex);
		if( g% 10000 == 0){
			printk(".");
		}
		buf[f++] = g++;
		f %= NBUF;
		V(&mutex);
		V(&full);
	}
}
void test_consumer(void){
	int get;
	while(1){
		P(&full);
		P(&mutex);
		get = buf[r++];
		assert(last == get-1 );
		last = get;
		r %= NBUF;
		V( &empty );
		V( &mutex );
	}
}
void test_setup(void){
	create_sem( &full, 0 );
	create_sem( &empty, NBUF );
	create_sem( &mutex, 1);
	int i;
	for(i=0; i<NR_PROD; i++){
		wakeup(create_kthread(test_producer));
	}
	for(i=0; i<NR_CONS; i++){
		wakeup(create_kthread(test_consumer));
	}
}
//消息机制的send和receive
void send(pid_t dest, Msg *m){
	/* different thread share the same address space, so can easily get the PCB */
	PCB *pcb = fetch_pcb(dest); 
//	Sem *msg_mutex = &pcb->msg_mutex;
	Sem *msg_full = &(pcb->msg_full); 
	lock();
	/* Access resource in a critical region : the other PCB can access too */
	/* ------------------------------------------------------------------- */
	if(list_empty (&pcb->msg_free) ){
		/* this is when the target thread has no free message buffer */
		if ( m->src == -2 ) printk("yes!\n");
		printk("the requested pid---%d\n, the requesting pid --- %d \n,type---%d\n",dest, m->src,m->type);
		assert(0);
	}
	ListHead *free = pcb->msg_free.next;
	list_del( free );
	list_add_before( &pcb->msg, free );
	volatile Msg *free_msg = list_entry( free, Msg, list);	
	free_msg->src = m->src;
	free_msg->dest = m->dest;
	free_msg->type = m->type;
	free_msg->req_pid = m->req_pid;
	free_msg->dev_id = m->dev_id;
	free_msg->buf = m->buf;
	free_msg->offset = m->offset;
	free_msg->len = m->len;
	unlock();
	V( msg_full ); 
}
void receive(pid_t src, Msg *m){
	ListHead *msg_list = &(current->msg);
//	Sem *msg_mutex = &(current->msg_mutex);
	Sem *msg_full = &(current->msg_full);
	while(true){

		P( msg_full );
//		P( msg_mutex );
		lock();
		//将消息存储到m所在位置	
		ListHead *index_list = msg_list->next;
		while( index_list != msg_list ){
			Msg* msg = list_entry(index_list, Msg, list);
			if( src == ANY ||msg->src == src ){
				m->src = msg->src;
				m->dest = msg->dest;
				m->type = msg->type;
				m->req_pid = msg->req_pid;
				m->dev_id = msg->dev_id;
				m->buf = msg->buf;
				m->offset = msg->offset;
				m->len = msg->len;
				
				list_del( index_list );
				list_add_before( &current->msg_free, index_list );
//				V( msg_mutex );
				unlock();
				return;
			}
			index_list = index_list->next;
		}
		//--end of 将消息
//		V( msg_mutex );
		unlock();
		V( msg_full );
	}
}

void A(){
	Msg m1, m2;
	m1.src = current->pid;
	int x = 0;
	while(1){
		if( x % 10000000 == 0){
			printk("a");
			send(pcb_struct[pcb_count-1].pid, &m1);
			receive(pcb_struct[pcb_count-1].pid, &m2);
		}
		x ++;
	}
}
void B(){
	Msg m1, m2;
	m1.src = current->pid;
	int x = 0;
	receive(pcb_struct[pcb_count-1].pid, &m2);
	while(1){
		if( x % 10000000 == 0 ){
			printk("b");
			send(pcb_struct[pcb_count-1].pid, &m1);
			receive(pcb_struct[pcb_count-1].pid, &m2);
		}
		x ++;
	}
}
void C(){
	Msg m1, m2;
	m1.src = current->pid;
	int x = 0;
	receive(pcb_struct[pcb_count-1].pid, &m2);
	while(1){
		if( x % 10000000 == 0 ){
			printk("c");
			send(pcb_struct[pcb_count-1].pid, &m1);
			receive(pcb_struct[pcb_count-1].pid, &m2);
		}
		x ++;
	}
}
void D(){
	Msg m1, m2;
	m1.src = current->pid;
	int x = 0;
	receive(pcb_struct[pcb_count-1].pid, &m2);
	while(1){
		if( x % 10000000 == 0 ){
			printk("d");
			send(pcb_struct[pcb_count-1].pid, &m1);
			receive(pcb_struct[pcb_count-1].pid, &m2);
		}
		x ++;
	}
}
void E(){
	Msg m1, m2;
	m2.src = current->pid;
	char c;
	while(1){
		receive(ANY, &m1);
		if(m1.src == 0){
			c = '|';
			m2.dest = 1;
		}else if(m1.src == 1){
			c = '/';
			m2.dest = 2;
		}else if(m1.src == 2){
			c = '-';
			m2.dest = 3;
		}else if(m1.src == 3){
			c = '-';
			m2.dest = 4;
		}else if(m1.src == 4){
			c = '\\';
			m2.dest = 0;
		}else{
			assert(0);
		}
		printk("\033[s\033[1000;1000H%c\033[u", c);
		send(m2.dest, &m2);
	}
}
