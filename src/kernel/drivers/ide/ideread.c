#include "kernel.h"
#include "ide.h"

static void 
geide(void){
	INTR;
	char mbr[512] ;
	const char name[] = "hda";
	INTR;
	dev_read( name, current->pid, mbr, 0, 512);	
	INTR;
	int j;
	for( j=0; j<512; j++){
		printk("%x",(int)mbr[j]);
	}
	printk("\n");
	INTR;
	while(1){
		printk("this is ide request running\n");
		asm volatile( "int $0x80 " );
	}
	
}

void init_geide(void) {
	NOINTR;
	wakeup(create_kthread(geide));
	NOINTR;
}
