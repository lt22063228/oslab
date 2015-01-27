#include "kernel.h"
#include "hal.h"
#include "ramdisk.h"

pid_t FM;
void do_read(int, uint8_t*, off_t, size_t);
static void fmd(void);
void init_fm(void) {
	PCB *p = create_kthread( fmd );
	FM = p->pid;
	wakeup(p);
}

static void
fmd( void ){
	Msg m;
	while(1){
		receive( ANY, &m );
		int file_name = m.dev_id;
		uint8_t *buf = m.buf;
		off_t offset = m.offset;
		size_t len = m.len;
		switch( m.type ){
			case FILE_READ:
				do_read(file_name, buf, offset, len );
				m.ret = len;
				m.dest = m.src;
				m.src = FM;
				send( m.dest, &m );
				break;
			default:	assert(0);
		}
	}
}

void do_read( int file_name, uint8_t *buf, off_t offset, size_t len  ){
	dev_read( "ram", current->pid, buf, file_name * NR_FILE_SIZE + offset, len);	
}
	
