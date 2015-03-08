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
#define EXIT 9
#define WAITPID 10
#define FILE_OPEN 11
#define FILE_CLOSE 12
#define FILE_WRITE 13
#define FILE_LSEEK 14
#define MAKE_FILE 15
#define LIST_DIR 16
#define USER_STACK_OFFSET (0xbffff000)

#define TYPE_REG 0
#define TYPE_DEV 1

#define ROOT_INODE 2

#define FILE_PLAIN 0
#define FILE_DIR 1

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
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
