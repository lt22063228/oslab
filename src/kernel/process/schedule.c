#include "kernel.h"
#include "hal.h"
extern pid_t MM;
void print_ready();
PCB idle, *current = &idle;
extern uint32_t count;
void schedule(void) {
	// print_ready();
	// printk("LEAVING:this is pid %d\n",current->pid);
	ListHead *list = ready.next;
	ListHead *now;
	NOINTR;
	assert( list != NULL && list->prev->next == list );
	assert( list != NULL && list->next->prev == list );
	// printk("Count:%d\n", count);
	// printk("1----------------------------\n");
	// print_ready();
	list_del( list );
	// printk("2----------------------------\n");
	// print_ready();
	list_add_before( &ready, list );
	// printk("3----------------------------\n");
	// print_ready();
	now = ready.next;
	current = list_entry( now, PCB, list ); 
	// printk("ENTERING:this is pid %d\n", current->pid);
	// if(count == 65) print_ready();
	// if(count == 66) print_ready();
	// print_ready();
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
