#ifndef __TIME_H__
#define __TIME_H__

#define HZ        100
#define NEW_TIMER -2
#define MSG_TIMER_UPDATE -1
	
typedef struct Time {
	int year, month, day;
	int hour, minute, second;
} Time;
inline long get_jiffy();
extern pid_t TIMER;
void get_time(Time *tm);

#endif
