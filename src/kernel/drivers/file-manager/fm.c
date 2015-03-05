#include "kernel.h"
#include "hal.h"
#include "ramdisk.h"
#define NR_SYSTEM_FDE 32
extern pid_t PM;
pid_t FM;
void do_read(int dev_type, int file_name, pid_t req_pid, uint8_t* buf, off_t offset, size_t len);
void do_write(int dev_type, int file_name, pid_t req_pid, uint8_t *buf, off_t offset, size_t len);
static void fmd(void);
static void file_read(Msg *msg);
static void file_open(Msg *msg);
static void file_close(Msg *msg);
static void file_write(Msg *msg);
static void file_lseek(Msg *msg);

/* file system */
typedef struct system_FDE{
	int is_used;
	int dev_type;
	int dev_id;
	off_t offset;
	int count;
} system_FDE;
system_FDE system_fdes[NR_SYSTEM_FDE];

void init_fm(void) {
	PCB *p = create_kthread( fmd );
	FM = p->pid;
	wakeup(p);
}

static void
fmd( void ){
	Msg msg;
	while(1){
		receive(ANY, &msg);
		switch( msg.type ){
			case FILE_READ:
				file_read(&msg);
				break;
			case FILE_OPEN:
				file_open(&msg);
				break;
			case FILE_CLOSE:
				file_close(&msg);
				break;
			case FILE_WRITE:
				file_write(&msg);
				break;
			case FILE_LSEEK:
				file_lseek(&msg);
				break;
			default:
				assert(0);
				break;
		}
	}
}
static void file_lseek(Msg *msg){
	int fd = msg->dev_id;
	int offset = msg->offset;
	pid_t req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	process_FDE *p_fde = pcb->fdes + fd;
	system_FDE *s_fde = system_fdes + p_fde->index; 
	s_fde->offset = offset;
	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
}
static void file_close(Msg *msg){
	int fd = msg->dev_id;
	PCB *pcb = fetch_pcb(msg->req_pid);
	process_FDE *p_fde = pcb->fdes + fd;
	p_fde->is_used = -1;
	fd = p_fde->index;
	system_FDE *s_fde = system_fdes + fd;
	s_fde->count --;
	if(s_fde->count == 0) s_fde->is_used = -1;
	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
}
static void file_write(Msg *msg){
	int fd = msg->dev_id;
	pid_t req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	process_FDE *p_fde = pcb->fdes + fd;
	system_FDE *s_fde = system_fdes + p_fde->index; 
	off_t offset = s_fde->offset;
	int dev_id = s_fde->dev_id;
	uint8_t *buf = msg->buf;
	size_t len = msg->len;
	int dev_type = s_fde->dev_type;
	do_write(dev_type, dev_id, req_pid, buf, offset, len);
	s_fde->offset += len;
	msg->ret = len;
	msg->dest = msg->src;
	msg->src = FM;
	send(msg->dest, msg);
}
static void file_read(Msg *msg){
	int fd = msg->dev_id;
	pid_t req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	process_FDE *p_fde = pcb->fdes + fd;
	system_FDE *s_fde = system_fdes + p_fde->index; 
	off_t offset = s_fde->offset;
	int dev_id = s_fde->dev_id;
	int dev_type = s_fde->dev_type;
	if(msg->src == PM){
		dev_id = msg->dev_id;
		offset = msg->offset;
		dev_type = TYPE_REG;
	}
	uint8_t *buf = msg->buf;
	size_t len = msg->len;
	do_read(dev_type, dev_id, req_pid, buf, offset, len);
	s_fde->offset += len;
	msg->ret = len;
	msg->dest = msg->src;
	msg->src = FM;
	send(msg->dest, msg);
}
static void file_open(Msg *msg){
	int i;
	int index = -1;
	int req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	int dev_id = msg->dev_id;
	for(i = 0; i < NR_SYSTEM_FDE; i++){
		system_FDE *fde = system_fdes + i;
		if(fde->is_used != 1){
			fde->is_used = 1;
			fde->offset = 0;
			fde->count = 1;
			fde->dev_id = dev_id;
			index = i;
			if(msg->src == PM){
				fde->dev_type = TYPE_DEV;
			}else{
				fde->dev_type = TYPE_REG;
			}
			break;
		}
	}
	if(i == NR_SYSTEM_FDE) assert(0);
	for(i = 0; i < NR_PROCESS_FDE; i++){
		process_FDE *p_fde = pcb->fdes + i;
		if(p_fde->is_used != 1){
			p_fde->is_used = 1;
			p_fde->index = index;	

			pid_t dest = msg->src;
			msg->src = current->pid;
			msg->ret = i;
			send(dest, msg);
			break;
		}	
	} 
	if(i == NR_PROCESS_FDE) assert(0);
}

void do_read(int dev_type, int dev_id, pid_t req_pid, uint8_t *buf, off_t offset, size_t len  ){
	/* dev_id is file index */
	char ttyx[5] = "ttyx\0";
	if(dev_type == TYPE_REG) dev_read("ram", req_pid, buf, dev_id * NR_FILE_SIZE + offset, len);	
	else if(dev_type == TYPE_DEV){
		ttyx[3] = dev_id + '0';
		dev_read(ttyx, req_pid, buf, -1, len);	
	}else{
		printk("UNKNOWN DEVICE TYPE----------------------\n");
	}
}

void do_write(int dev_type, int dev_id, pid_t req_pid, uint8_t *buf, off_t offset, size_t len){
	char ttyx[5] = "ttyx\0";
	if(dev_type == TYPE_REG) dev_write("ram", req_pid, buf, dev_id * NR_FILE_SIZE + offset, len);	
	else if(dev_type == TYPE_DEV){
		int i = 0;
		while(*(buf + i)) i++;
		*(buf + i) = '\n';
		ttyx[3] = dev_id + '0';
		dev_write(ttyx, req_pid, buf, -1, i + 1);	

	}else{
		printk("UNKNOWN DEVICE TYPE----------------------\n");
	}
}
	
