#include "kernel.h"
#include "hal.h"
extern pid_t MM;
void print_ready();
PCB idle, *current = &idle;
extern uint32_t count;
void schedule(void) {
	ListHead *list = ready.next;
	ListHead *now;
	NOINTR;
	assert( list != NULL && list->prev->next == list );
	assert( list != NULL && list->next->prev == list );
	list_del( list );
	list_add_before( &ready, list );
	now = ready.next;
	current = list_entry( now, PCB, list ); 
	if(current->pid == 12 || current->pid == 11){
		printk("I'AM PROC %d\n", current->pid);
	}
	write_cr3( current->cr3 );
	// set_tss_esp0((uint32_t)current->tf);//(((TrapFrame*)(current->tf))->esp);
}
void print_ready(){
	ListHead *cur = ready.next;
	while(cur != &ready){
		pid_t cur_pid = list_entry(cur, PCB, list)->pid;
		printk("%d <->",cur_pid);
		cur = cur->next;
	}
	printk("\n");
}
