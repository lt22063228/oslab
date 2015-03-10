#include "kernel.h"
#include "hal.h"
#include "ramdisk.h"
#define NR_SYSTEM_FDE 32
extern pid_t PM;
pid_t FM;
inode_t root_dir;

/* file-system */

#define NR_INODE (36)
#define NR_BLOCK (128)
#define SIZE_OF_BLOCK (4 * 1024)

#define INODE_BITMAP_BASE 0
#define BLOCK_BITMAP_BASE (NR_INODE * 1)
#define INODE_BASE (BLOCK_BITMAP_BASE + NR_BLOCK * 1)
#define BLOCK_BASE (INODE_BASE + NR_INODE * 128)
#define SIZE_OF_IMAGE (BLOCK_BASE + NR_BLOCK * SIZE_OF_BLOCK)
#define INDEX_SIZE sizeof(int)
#define NR_INDEX (SIZE_OF_BLOCK / INDEX_SIZE)
#define ONE_INDEX_BASE 12
#define TWO_INDEX_BASE (ONE_INDEX_BASE + NR_INDEX)
#define THREE_INDEX_BASE (TWO_INDEX_BASE + NR_INDEX * NR_INDEX)


typedef int inode_t;
typedef int block_t;

uint8_t inode_bitmap[NR_INODE];
uint8_t block_bitmap[NR_BLOCK];
typedef struct dir_entry{
	char filename[32];
	inode_t inode;
} dir_entry;
typedef struct inode_entry{
	size_t size;
	int type;
	int dev_id;
	int link;
	char pad[52];
	block_t index[15];
} inode_entry;

inode_t root_dir;

static char blk[SIZE_OF_BLOCK];

void ram_read(int base, uint8_t *buf, size_t ele_size, int count);
void ram_write(int base, uint8_t *buf, size_t ele_size, int count);
void write_inode(inode_t index, inode_entry *data);
void read_inode(inode_t index, inode_entry *data);
void read_block(block_t index, void *data);
void write_block(block_t index, void *data);
inode_t get_free_inode(uint8_t *inode_bitmap);
block_t get_free_block(uint8_t *block_bitmap);
inode_t make_file(char *name, inode_t parent_dir, int file_type);
int lsdir(char *dir, char *buf, inode_t current_dir);
inode_t get_file(char *path, inode_t parent_dir);
int check_file_type(inode_t file);
inode_t find_file(char *name, inode_t dest);
void get_path_and_name(char *dir, char *path, char *name);
inode_t mkdir(char *dir);
inode_t chdir(char *dir, pid_t req_pid);
int remove_file(inode_t file_index);
int rmdir(char *dir, int idx);

/* end-of-file-system */

void do_read(int dev_type, int file_name, pid_t req_pid, uint8_t* buf, off_t offset, size_t len);
void do_write(int dev_type, int file_name, pid_t req_pid, uint8_t *buf, off_t offset, size_t len);
block_t index_block(int seq_num, inode_t parent_dir, block_t new_file);
static void fmd(void);
static void file_read(Msg *msg);
static void file_open(Msg *msg);
static void file_close(Msg *msg);
static void file_write(Msg *msg);
static void file_lseek(Msg *msg);
static void sys_make_file(Msg *msg);
static void sys_lsdir(Msg *msg);
static void sys_mkdir(Msg *msg);
static void sys_chdir(Msg *msg);
static void sys_rmdir(Msg *msg);

/* file system */
typedef struct system_FDE{
	int is_used;
	int dev_type;
	int dev_id;
	off_t offset;
	int count;
	inode_t file_inode;
} system_FDE;
system_FDE system_fdes[NR_SYSTEM_FDE];

void init_fm(void) {
	PCB *p = create_kthread( fmd );
	FM = p->pid;
	wakeup(p);

}

static pid_t request_pid;
static inode_t cur_directory;
static void
fmd( void ){

	ram_read(INODE_BITMAP_BASE, inode_bitmap, sizeof(char), (BLOCK_BITMAP_BASE - INODE_BITMAP_BASE));
	ram_read(BLOCK_BITMAP_BASE, block_bitmap, sizeof(char), (INODE_BASE - BLOCK_BITMAP_BASE));
	root_dir = ROOT_INODE;

	Msg msg;
	while(1){
		receive(ANY, &msg);
		request_pid = msg.req_pid;
		PCB *pcb = fetch_pcb(request_pid);
		cur_directory = pcb->current_dir;
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
			case MAKE_FILE:
				sys_make_file(&msg);
				break;
			case LIST_DIR:
				sys_lsdir(&msg);
				break;
			case MKDIR:
				sys_mkdir(&msg);
				break;
			case CHDIR:
				sys_chdir(&msg);
				break;
			case RMDIR:
				sys_rmdir(&msg);
				break;
			default:
				assert(0);
				break;
		}
	}
}
static void sys_rmdir(Msg *msg){
	char *name = msg->buf;
	msg->ret = rmdir(name, 0);

	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
}
static void sys_chdir(Msg *msg){
	char *name = msg->buf;
	msg->ret = chdir(name, msg->req_pid);

	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
}
static void sys_mkdir(Msg *msg){

	char *name = msg->buf;
	msg->ret = mkdir(name);	

	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
}
static void sys_make_file(Msg *msg){

	pid_t req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	inode_t current_dir = pcb->current_dir;

	char *name = msg->buf;
	int type = msg->offset;
	msg->ret = make_file(name, current_dir, type);	

	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
}

static void sys_lsdir(Msg *msg){

	char *path = (char*)msg->offset;
	char *buf = (char*)msg->buf;
	pid_t req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	inode_t current_dir = pcb->current_dir;

	lsdir(path, buf, current_dir);
	msg->buf = buf;
	pid_t dest = msg->src;
	msg->src = current->pid;
	send(dest, msg);
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

	int req_pid = msg->req_pid;
	PCB *pcb = fetch_pcb(req_pid);
	int dev_id = msg->dev_id;
	inode_t file_inode = msg->offset;
	int i, index;
	for(i = 0; i < NR_SYSTEM_FDE; i++){
		system_FDE *fde = system_fdes + i;
		if(fde->is_used != 1){
			fde->is_used = 1;
			fde->offset = 0;
			fde->count = 1;
			fde->dev_id = dev_id;
			fde->file_inode = file_inode;
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
			/* change to the inode of the target file */
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

void ram_write(int base, uint8_t *buf, size_t ele_size, int count){
	do_write(TYPE_REG, 0, current->pid, buf, base, ele_size * count);
}

void ram_read(int base, uint8_t *buf, size_t ele_size, int count){
	do_read(TYPE_REG, 0, current->pid, buf, base, ele_size * count);
}
	
void write_inode(inode_t index, inode_entry *data){
	int offset = INODE_BASE + sizeof(inode_entry) * index;
	ram_write(offset, (uint8_t*)data, sizeof(inode_entry), 1);
}

void read_inode(inode_t index, inode_entry *data){
	int offset = INODE_BASE + sizeof(inode_entry) * index;
	ram_read(offset, (uint8_t*)data, sizeof(inode_entry), 1);
}

void write_block(block_t index, void *data){
	int offset = BLOCK_BASE + SIZE_OF_BLOCK * index;
	ram_write(offset, data, sizeof(char), SIZE_OF_BLOCK);
}

void read_block(block_t index, void *data){
	int offset = BLOCK_BASE + SIZE_OF_BLOCK * index;
	ram_read(offset, data, sizeof(char), SIZE_OF_BLOCK);
}

inode_t get_free_inode(uint8_t *inode_bitmap){
	int i;
	for(i = 0; i < NR_INODE; i++){
		if(inode_bitmap[i] == 0){
			inode_bitmap[i] = 1;
			return i;
		}
	}
	return -1;
}

block_t get_free_block(uint8_t *block_bitmap){
	int i;
	for(i = 0; i < NR_BLOCK; i++){
		if(block_bitmap[i] == 0){
			block_bitmap[i] = 1;
			return i;
		}
	}
	return -1;
}

block_t index_block(int seq_num, inode_t parent_dir, block_t new_file){
	inode_entry inode;
	read_inode(parent_dir, &inode);
	block_t *index = inode.index;	
	block_t res = -1;
	int offset;
	if(seq_num >= 0 && seq_num < ONE_INDEX_BASE){
		if(new_file != -1){
			*(index + seq_num) = new_file;
			write_inode(parent_dir, &inode);
		}else
			res = *(index + seq_num);
	}else if(seq_num < TWO_INDEX_BASE){
		seq_num -= TWO_INDEX_BASE;
		read_block(index[12], blk);
		if(new_file != -1){
			*((block_t *)blk + seq_num * INDEX_SIZE) = new_file;
			write_block(index[12], blk);
		}else
			res = *((block_t *)blk + seq_num * INDEX_SIZE);
	}else if(seq_num < THREE_INDEX_BASE){
		seq_num -= TWO_INDEX_BASE;
		offset = seq_num / NR_INDEX;
		read_block(index[13], blk);
		res = *((block_t *)blk + offset * INDEX_SIZE);
		seq_num -= offset * (NR_INDEX); 
		read_block(res, blk);
		if(new_file != -1){
			*((block_t *)blk + offset * INDEX_SIZE) = new_file;
			write_block(index[12], blk);
		}else
			res = *((block_t *)blk + offset * INDEX_SIZE);
	}else{
		seq_num -= THREE_INDEX_BASE;
		offset = seq_num / (NR_INDEX * NR_INDEX);
		read_block(index[14], blk);
		res = *((block_t *)blk + offset * INDEX_SIZE);
		seq_num -= offset * (NR_INDEX * NR_INDEX);
		offset = seq_num / NR_INDEX;
		read_block(res, blk);
		res = *((block_t *)blk + offset * INDEX_SIZE);
		seq_num -= offset * NR_INDEX;
		read_block(res, blk);
		if(new_file != -1){
			*((block_t *)blk + offset * INDEX_SIZE) = new_file;
			write_block(index[12], blk);
		}else
			res = *((block_t *)blk + offset * INDEX_SIZE);
	}
	if(new_file == -1) return res;
	else return 1;
}

inode_t make_file(char *name, inode_t parent_dir, int file_type){
	inode_t inode_index = get_free_inode(inode_bitmap);
	inode_entry inode;
	inode.size = 0;
	inode.type = file_type;

	/* if new file is a directory, add . and .. entry */
	if(file_type == FILE_DIR){
		inode.size = 2 * sizeof(dir_entry);
		dir_entry dirs[2];
		strcpy(dirs[0].filename, ".");
		dirs[0].inode = inode_index;
		strcpy(dirs[1].filename, "..");
		dirs[1].inode = parent_dir;
		inode.index[0] = get_free_block(block_bitmap);
		memcpy(blk, dirs, inode.size);
		write_block(inode.index[0], blk);
	}

	write_inode(inode_index, &inode);

	/* append the dir_entry to the back of the directory */
	inode_entry cur_dir;
	read_inode(parent_dir, &cur_dir);
	int len = cur_dir.size;

	if(len % SIZE_OF_BLOCK == 0){
		/* add a new blk for the parent directory */
		block_t index = get_free_block(block_bitmap);
		index_block(len / SIZE_OF_BLOCK, parent_dir, index);
	}

	cur_dir.size += sizeof(dir_entry);
	int residual = (SIZE_OF_BLOCK -  cur_dir.size % SIZE_OF_BLOCK);
	if(residual < sizeof(dir_entry)){
		cur_dir.size += residual;
	}

	write_inode(parent_dir, &cur_dir);
	// int num = len % unit == 0 ? (len / unit) : (len / unit + 1);
	int seq_num = len / SIZE_OF_BLOCK;
	dir_entry dir_item;	
	strcpy(dir_item.filename, name);
	dir_item.inode = inode_index;
	/* looking for the index */
	block_t block_index = index_block(seq_num, parent_dir, -1);
	int offset = len - SIZE_OF_BLOCK * seq_num;
	read_block(block_index, blk);
	memcpy(blk + offset, &dir_item, sizeof(dir_item));
	write_block(block_index, blk);

	return inode_index;
}

int lsdir(char *dir, char *buf, inode_t current_dir){
	inode_t dir_index = get_file(dir, current_dir);
	if(check_file_type(dir_index) != FILE_DIR) return -1;
	inode_entry dir_inode;
	read_inode(dir_index, &dir_inode);

	int len = dir_inode.size;
	int num = len / SIZE_OF_BLOCK;
	num = len % SIZE_OF_BLOCK == 0 ? num : (num + 1);	
	int unit_num = SIZE_OF_BLOCK / sizeof(dir_entry);
	int i;
	for(i = 0; i < num; i++){
		block_t ind = index_block(i, dir_index, -1);
		read_block(ind, blk);
		int end = len >= SIZE_OF_BLOCK ? unit_num : len / sizeof(dir_entry); 
		dir_entry *p_dir_item = (dir_entry*)blk;
		dir_entry dir_item;
		int j;
		for(j = 0; j < end; j++){
			memcpy(&dir_item, p_dir_item + j, sizeof(dir_entry));
			strcpy(buf, dir_item.filename);
			buf += strlen(dir_item.filename);
			*buf = '\n';
			buf ++;
		}
		len -= SIZE_OF_BLOCK;
	}
	return 1;
}

inode_t get_file(char *path, inode_t parent_dir){

	inode_t dest = parent_dir;
	char name[32];

	if(*path == '/'){
		dest = root_dir;
		path ++;
	}
	int len = strlen(path);

	/* remove the trailing '\' */
	if(*(path + len - 1) == '/'){
		*(path + len - 1) = '\0';
	}

	while(*path != '\0'){
		if(check_file_type(dest) != FILE_DIR) return -1;
		int i;
		for(i = 0; *path != '/' && *path != '\0'; i++){
			*(name + i) = *path;
			path ++;
		}
		*(name + i) = '\0';
		dest = find_file(name, dest);
		if(dest == -1) return -1;
	}
	return dest;
}

int check_file_type(inode_t file){
	if(file == -1) return -1;
	inode_entry inode;
	read_inode(file, &inode);
	return inode.type;	
}
inode_t find_file(char *name, inode_t dest){
	inode_entry dir;
	read_inode(dest, &dir);
	int num = dir.size / SIZE_OF_BLOCK + 1;
	int len = dir.size;
	int i;
	for(i = 0; i < num; i++){
		block_t ind = index_block(i, dest, -1);
		read_block(ind, blk);
		int end = len < SIZE_OF_BLOCK ?  len / sizeof(dir_entry) : SIZE_OF_BLOCK / sizeof(dir_entry); 
		dir_entry *dir_e = (dir_entry*)blk;
		int j;
		for(j = 0; j < end; j++){
			if(strcmp((dir_e + j)->filename, name) == 0){
				return (dir_e + j)->inode;
			}
		}
		len -= SIZE_OF_BLOCK;
	}
	return -1;
}

inode_t mkdir(char *dir){
	inode_t parent;
	char name[32];
	char path[64];
	get_path_and_name(dir, path, name);
	parent = get_file(path, cur_directory);
	if(check_file_type(parent) == FILE_DIR)
		return make_file(name, parent, FILE_DIR);
	else
		return -1;
}

void get_path_and_name(char *dir, char *path, char *name){
	int i, j = 0;
	for(i = 0; *(dir + i) != '\0'; i++)
		if(*(dir + i) == '/') j = i+1;
	// strncpy(path, dir, j + 1);
	memcpy(path, dir, j);
	path[j] = '\0';
	strcpy(name, dir+j);
}

inode_t chdir(char *dir, pid_t req_pid){

	PCB *pcb = fetch_pcb(req_pid);

	inode_t res = get_file(dir, pcb->current_dir);
	if(check_file_type(res) == FILE_DIR){
		pcb->current_dir = res;
		return res;
	}else{
		return -1;
	}
}

static char str_buf[2048] = {0};

int rmdir(char *dir, int idx){
	inode_t dest = get_file(dir, cur_directory);
	char name[32];
	char path[64];
	get_path_and_name(dir, path, name);
	if(check_file_type(dest) == FILE_DIR){

		char child_path[64];
		lsdir(dir, str_buf + idx, cur_directory);
		char *source = str_buf + idx * 256;
		char *end = str_buf + idx * 256 + 256;
		// if(source >= (str_buf + 2048) || end >= (str_buf + 2048))
		// 	printk("\n\n\nfm.c:640 ..........................\n\n\n");
		int path_len = strlen(path);

		while(*source != '\0' && source < end){

			strcpy(child_path, path);
			*(child_path + path_len) = '/';
			strcpy(child_path + path_len + 1, source);
			source += strlen(source) + 1;

			rmdir(child_path, idx + 1);
		}

		memset(str_buf + idx, 0, 256);

	}else if(check_file_type(dest) == FILE_PLAIN){
		remove_file(dest);	
	}else{
		return -1;
	}

	/* update the entry of the parent directory */
	inode_t parent_index = get_file(path, cur_directory);
	inode_entry parent;
	read_inode(parent_index, &parent);
	int len = parent.size;
	parent.size -= sizeof(dir_entry);
	write_inode(parent_index, &parent);
	int block_num = len / SIZE_OF_BLOCK + 1;

	int i;
	for(i = 0; i < block_num; i ++){
		block_t block_index =  index_block(i, parent_index, -1);
		read_block(block_index, blk);
		dir_entry *dir_e = (dir_entry*)blk;
		int num_entry = (len < SIZE_OF_BLOCK ? len / sizeof(dir_entry) : (SIZE_OF_BLOCK / sizeof(dir_entry)));
		int found = 0;
		int j;
		for(j = 0; j < num_entry; j++){
			if(strcmp((dir_e + j)->filename, name) == 0){
				found = 1;
				break;
			}	
		}
		if(found == 1){
			int temp_len = blk + SIZE_OF_BLOCK - (char*)(dir_e + j + 1);
			memcpy(dir_e + j, dir_e + j + 1, temp_len);
			write_block(block_index, blk);
			break;
		}
	}

	return 1;
}
int remove_file(inode_t file_index){
	inode_entry file;
	read_inode(file_index, &file);
	int num = file.size / SIZE_OF_BLOCK;
	num = file.size % SIZE_OF_BLOCK == 0 ? num : (num + 1);	
	int i;
	for(i = 0; i < num; i++){
		block_t b_index = index_block(i, file_index, -1);
		block_bitmap[b_index] = 0;
	}
	inode_bitmap[file_index] = 0;
	return 1;
}