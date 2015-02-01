#ifndef __HAL_H__
#define __HAL_H__

#include "common.h"
#include "adt/list.h"

#define DEV_READ 1
#define DEV_WRITE 2
#define FILE_READ 3
#define NEW_PAGE 4
#define NEW_PCB 5
#define MAP_KERNEL 6
#define FORK 7
#define EXEC 8

/* message from outside interruption, no pid */
#define MSG_HARD_INTR 100

void add_irq_handle(int, void (*)(void));

typedef struct Device {
	const char *name;
	pid_t pid;
	int dev_id;
	
	ListHead list;
} Dev;

size_t dev_read(const char *dev_name, pid_t reqst_pid, void *buf ,off_t offset, size_t len);
size_t dev_write(const char *dev_name, pid_t reqst_pid, void *buf ,off_t offset, size_t len);
void init_driver();
void hal_register(const char *name, pid_t pid, int dev_id);
void hal_get_id(const char *name, pid_t *pid, int *dev_id);
void hal_list(void);

void copy_from_kernel(PCB* pcb, void* dest, void* src, int len);
void copy_to_kernel(PCB* pcb, void* dest, void* src, int len);
void strcpy_to_kernel(PCB* pcb, char* dest, char* src);
void strcpy_from_kernel(PCB* pcb, char* dest, char* src);
void copy_from_kernel_mine(PCB* pcb, void* dest, void* src, int len);
void set_from_kernel_mine(PCB* pcb, void* dest, uint8_t data, int len);

#endif
