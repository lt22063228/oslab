#ifndef __PROCESS_H__
#define __PROCESS_H__
#define KSTACK_SIZE 4096
#define ANY -1
#define MSG_BUFF_SIZE 256 
#define IF_MASK 0x200
#define INTR assert(read_eflags()&IF_MASK)
#define NOINTR assert(~read_eflags()&IF_MASK)
typedef struct Semaphore{
	int token;
	ListHead block;
}Sem;
typedef struct Message {
	pid_t src, dest;
	union {
		int type;
		int ret;
	};
	union {
		int i[5];
		struct {
			pid_t req_pid;
			int dev_id;
			void *buf;
			off_t offset;
			size_t len;
		};
	};
	ListHead list;
} Msg;
typedef struct PCB {
	void *tf;
	uint8_t kstack[KSTACK_SIZE];
	ListHead list;
	pid_t pid;
	ListHead msg;
	Sem msg_mutex;
	Sem msg_full;
	int lock_count;
	bool if_flag;
	Msg msg_buff[MSG_BUFF_SIZE];
	ListHead msg_free;
	CR3* cr3;
} PCB;
extern PCB *current;
extern ListHead ready, free_pcb, block;
extern ListHead lock_queue;
void wakeup(PCB *p);
void sleep(ListHead *block, PCB *pcb);

PCB* create_kthread( void *fun);
void create_sem( Sem* s, int count );
void send( pid_t dest, Msg *m);
void receive( pid_t src, Msg *m);
PCB* fetch_pcb(pid_t req_pid);
void lock();
void unlock();
void print_ready();
void init_pcb(pid_t req_pid);
#endif
