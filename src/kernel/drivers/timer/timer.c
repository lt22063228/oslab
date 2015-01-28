#include "kernel.h"
#include "x86/x86.h"
#include "hal.h"
#include "time.h"
#include "string.h"


#define PORT_TIME 0x40
#define PORT_RTC  0x70
#define FREQ_8253 1193182
#define SIZE_OF_TIMER 16

pid_t TIMER;
static long jiffy = 0;
static Time rt;

static void update_jiffy(void);
static void init_i8253(void);
static void init_rt(void);
static void timer_driver_thread(void);

void init_timer(void) {
	init_i8253();
	init_rt();
	/* top half */
	add_irq_handle(0, update_jiffy);
	/* bottom half */
	PCB *p = create_kthread(timer_driver_thread);
	TIMER = p->pid;
	wakeup(p);
	hal_register("timer", TIMER, 0);
}
static Msg timer_buff[SIZE_OF_TIMER];	
static ListHead free_timer_buff;
static ListHead used_timer_buff;
static void
timer_driver_thread(void) {
	static Msg m;
	
	list_init( &free_timer_buff );
	list_init( &used_timer_buff );
	int i;
	for( i=0; i < SIZE_OF_TIMER; i++ ){
		list_add_before( &free_timer_buff, &timer_buff[i].list );
	}
	while (true) {
		ListHead *ptr;
		receive(ANY, &m);
		ListHead *lh;
		if(m.src == MSG_HARD_INTR) {
			switch (m.type) {
				case MSG_TIMER_UPDATE:
//					check_timer();
//					中断必须是每秒发送一个这个请求，而不是每个时钟中断
					list_foreach( ptr, &used_timer_buff ){
						Msg *msg = list_entry( ptr, Msg, list );
						msg->i[0] --;
						if( msg->i[0] == 0 ){
							ListHead *next = ptr->next;
							list_del( ptr );
							list_add_before( &free_timer_buff , ptr );
							send( msg->dest, &(*msg) );
							ptr = next;	
							continue;
						}
					}
					break;
				default: assert(0);
			}
		}else{
			switch (m.type) {
				case NEW_TIMER:
					if( list_empty( &free_timer_buff ) ) assert(0);
					lh = free_timer_buff.next;
					list_del(lh);
					list_add_before( &used_timer_buff, lh );
					Msg *msg = list_entry( lh, Msg, list );
					msg->src = current->pid;
					msg->dest = m.src;
					msg->type = m.type;
					msg->i[0] = m.i[0];
					break;

				default: assert(0);
			}
		}
	}
}

long
get_jiffy() {
	return jiffy;
}

static int
md(int year, int month) {
	bool leap = (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
	static int tab[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return tab[month] + (leap && month == 2);
}

static void
update_jiffy(void) {
	jiffy ++;
	if (jiffy % HZ == 0) {
		rt.second ++;
		if (rt.second >= 60) { rt.second = 0; rt.minute ++; }
		if (rt.minute >= 60) { rt.minute = 0; rt.hour ++; }
		if (rt.hour >= 24)   { rt.hour = 0;   rt.day ++;}
		if (rt.day >= md(rt.year, rt.month)) { rt.day = 1; rt.month ++; } 
		if (rt.month >= 13)  { rt.month = 1;  rt.year ++; }
		Msg msg;
		msg.src = MSG_HARD_INTR;
		msg.type = MSG_TIMER_UPDATE;
		msg.dest = TIMER;
		send( TIMER, &msg );
	}
}

static void
init_i8253(void) {
	int count = FREQ_8253 / HZ;
	assert(count < 65536);
	out_byte(PORT_TIME + 3, 0x34);
	out_byte(PORT_TIME, count & 0xff);
	out_byte(PORT_TIME, count >> 8);	
}

static void
init_rt(void) {
	memset(&rt, 0, sizeof(Time));
	/* Optional: Insert code here to initialize current time correctly */
	out_byte(0x70, 0x00);
	rt.second = in_byte(0x71);
	out_byte(0x70, 0x02);
	rt.minute = in_byte(0x71);
	out_byte(0x70, 0x04);
	rt.hour = in_byte(0x71);
	out_byte(0x70, 0x07);
	rt.day = in_byte(0x71);
	out_byte(0x70, 0x08);
	rt.month = in_byte(0x71);
	out_byte(0x70, 0x09);
	rt.year = in_byte(0x71);
}

void 
get_time(Time *tm) {
	memcpy(tm, &rt, sizeof(Time));
}
