#include "kernel.h"
#include "hal.h"
extern pid_t MM;
void print_ready();
PCB idle, *current = &idle;
void
schedule(void) {
	// print_ready();
	ListHead *list = ready.next;
	ListHead *now;
	assert( list != NULL && list->prev->next == list );
	assert( list != NULL && list->next->prev == list );
	list_del( list );
	list_add_before( &ready, list );
	now = ready.next;
	current = list_entry( now, PCB, list ); 
	if(current->pid == 10){
		printk("this is pid 10\n");
	}
	if(current->pid == 11){
		printk("this is pid 11\n");
	}
	write_cr3( current->cr3 );
	// set_tss_esp0((uint32_t)current->tf);//(((TrapFrame*)(current->tf))->esp);
}
static uint32_t count = 0;
void print_ready(){
	ListHead *cur = ready.next;
	printk("%d: ",count);
	while(cur != &ready){
		pid_t cur_pid = list_entry(cur, PCB, list)->pid;
		printk("%d <->",cur_pid);
		cur = cur->next;
	}
	count ++;
	printk("\n");
}
