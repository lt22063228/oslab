#include "kernel.h"
#include "tty.h"

static int tty_idx = 1;
extern pid_t TIMER;
extern pid_t FM;
static void
getty(void) {
	char name[] = "tty0", buf[256];
	lock();
	name[3] += (tty_idx ++);
	unlock();
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
		// dev_read( name, current->pid, buf, 0, 255 );	
		INTR;
		char c;
		for( i=0; (c = buf[i] ) != '\0'; i++){
			if( c >= 'a' && c <='z'){
				c = c - 'a' + 'A';
				buf[i] = c;
			}
		}	
		INTR;
		/* it is just changing the data stored in the corresponding tty
		 * the real pixel update happen periodically */
		// dev_write( name, current->pid, buf, 0, 255);
		INTR;
	}
}

void
init_getty(void) {
	int i;
	for(i = 0; i < NR_TTY; i ++) {
		NOINTR;
		wakeup(create_kthread(getty));
		NOINTR;
	}
}


