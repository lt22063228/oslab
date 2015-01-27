#include "kernel.h"
#include "tty.h"

static int tty_idx = 1;
extern pid_t TIMER;
extern pid_t FM;
void create_process();
static void
getty(void) {
	char name[] = "tty0", buf[256];
	
//	Msg m;
//	m.src = current->pid;
//	m.type = -2;
//	m.i[0] = 300;
//	send( TIMER, &m );
//	lock();
//	printk("set timer for 5 seconds, pid:%d\n",current->pid);
//	unlock();
//	receive( TIMER, &m );
//	lock();
//	printk("get respo for 5 seconds, pid:%d\n", current->pid);
//	unlock();
//	INTR;
	lock();
	name[3] += (tty_idx ++);
	unlock();
//	INTR;
//	m.src = current->pid;
//	m.type = FILE_READ;
//	m.dev_id = 0;
//	m.buf = buf;
//	m.offset = 0;
//	m.len = 256;
//	send( FM, &m ); 
//	receive( FM, &m );
//	lock();
//	printk("pid:%d----%s",current->pid,buf);
//	unlock();
	if(tty_idx == 4){
		create_process();
	}
	while(1) {
		/* Insert code here to do these:
		 * 1. read key input from ttyd to buf (use dev_read())
		 * 2. convert all small letters in buf into capitcal letters
		 * 3. write the result on screen (use dev_write())
		 */
		int i;
		for( i=0; i<256; i++){
			buf[i] = '\0';
		}
		INTR;
		dev_read( name, current->pid, buf, 0, 255 );	
		INTR;
		char c;
		for( i=0; (c = buf[i] ) != '\0'; i++){
			if( c >= 'a' && c <='z'){
				c = c - 'a' + 'A';
				buf[i] = c;
			}
		}	
		INTR;
		dev_write( name, current->pid, buf, 0, 255);
		INTR;
	}
}

void
init_getty(void) {
	int i;
	for(i = 0; i < NR_TTY; i ++) {
//	for(i = 0; i < 2; i ++) {
		NOINTR;
		wakeup(create_kthread(getty));
		NOINTR;
	}
}


