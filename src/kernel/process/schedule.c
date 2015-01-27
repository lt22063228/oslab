#include "kernel.h"

PCB idle, *current = &idle;
void
schedule(void) {
	/* implement process/thread schedule here */
	ListHead *list = ready.next;
	list = list->next;
	ListHead *now;
	if( list != &ready ){
		assert( list != NULL && list->prev->next == list );
		assert( list != NULL && list->next->prev == list );
		list_del( list );
		list_add_before( &ready, list );
		now = ready.next->next;
	}else{
		now = ready.next;
	}
	current = list_entry( now, PCB, list ); 
	if(current->pid == 10){
		printk("haha\n");
	}
	if(current != &idle){	
		write_cr3( current->cr3 );
	}
}
