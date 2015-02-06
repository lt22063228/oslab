#include "kernel.h"
#include "hal.h"
extern pid_t MM;
void print_ready();
PCB idle, *current = &idle;
extern uint32_t count;
// uint32_t *esp_addr;
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
	TrapFrame *tf = (TrapFrame*)current->tf;
	if(current->pid == 10){
		// PCB *next = list_entry(current->list.next, PCB, list);
		// if(count > 167){
		// 	printk("\n\n");
		// }
		// printk("th esp is %d, address is %d\n", tf->esp, &(tf->esp));
		// esp_addr = &(tf->esp);
		tf ++;
		set_tss_esp0((uint32_t)tf);//(((TrapFrame*)(current->tf))->esp);
	}
	write_cr3( current->cr3 );
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
