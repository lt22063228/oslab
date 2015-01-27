#include "kernel.h"

void init_hal();
void init_timer();
void init_tty();
void init_ide();
void init_ramdisk();
void init_fm();
void init_mm();
void init_pm();

void init_driver() {
	init_hal();
	init_timer();
//	init_ide();
	init_ramdisk();
	init_fm();
	init_mm();
	init_pm();
	init_tty();
	hal_list();
}
