#include <stdio.h>
#include <string.h>
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

#define FILE_PLAIN 0
#define FILE_DIR 1
typedef int inode_t;
typedef int block_t;
typedef unsigned int size_t;
static char data[SIZE_OF_IMAGE] = {'\0'};
char inode_bitmap[NR_INODE];
char block_bitmap[NR_BLOCK];
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
inode_t current_dir;
inode_t root_dir;

inode_t make_root();
int chdir(char *dir);
inode_t get_file(char *path, inode_t from);
int check_file_type(inode_t file);
inode_t find_file(char *name, inode_t dest);
void get_path_and_name(char *dir, char *path, char *name);
inode_t mkdir(char *dir);
int lsdir(char *dir, char *buf);
int rmdir(char *dir);
int remove_file(inode_t file_index);
inode_t make_file(char *name, inode_t dest);
inode_t make_dir(char *name, inode_t dest);
block_t index_block(int seq_num, inode_t inode_index, block_t index);
void write_inode(inode_t index, inode_entry *data, int count);
void read_inode(inode_t index, inode_entry *data, int count);
void read_block(block_t index, void *data);
void write_block(block_t index, void *data);
inode_t get_free_inode(char *inode_bitmap);
block_t get_free_block(char *block_bitmap);
static void file_read(int file_inode, int offset, char *buf, int len);
static void file_write(int file_inode, int offset, char *buf, int len);


void print_dir(char *buf){
	char *source = buf;
	char *end = buf + 20*SIZE_OF_BLOCK;
	while(*source != '\0' && source < end){
		printf("%s\n", source);
		source += (strlen(source) + 1);
	}
}
int main(){
	// char buf1[10] = {"hahaha\n"};
	// char buf2[10] = {"woqu\n"};
	// FILE *fp = fopen("mydisk2.img", "wb");
	// fwrite(buf1, sizeof(char), 10, fp);
	// fclose(fp);

	// fp = fopen("mydisk2.img", "r+b");
	// fseek(fp, 10, SEEK_SET);
	// fwrite(buf2, sizeof(char), 10, fp);
	// fclose(fp);


	

	FILE *fp = fopen("mydisk.img", "wb");
	fwrite(data, sizeof(char), SIZE_OF_IMAGE, fp);
	fclose(fp);

	fp = fopen("mydisk.img", "rb");
	fread(inode_bitmap, sizeof(char), (BLOCK_BITMAP_BASE - INODE_BITMAP_BASE), fp);	
	fread(block_bitmap, sizeof(char), (INODE_BASE - BLOCK_BITMAP_BASE), fp);
	fclose(fp);

	root_dir = make_root();
	current_dir = root_dir;
	// mkdir("/test_dir");
	// make_file("test_file", current_dir);
	mkdir("bin");
	chdir("bin");
	char dir[10] = {"/bin/"};
	inode_t parent_idx = get_file(dir, current_dir);
	char file[10] = {"sh"};
	inode_t target_idx = make_file(file, current_dir);
	char block[SIZE_OF_BLOCK];
	fp = fopen("shell.img", "rb");
	int n = 1;
	int offset = 0;
	while(n != 0){
		printf("ready to write one block\n");
		n = fread(block, sizeof(char), SIZE_OF_BLOCK, fp);
		file_write(target_idx, offset, block, n);
		file_read(target_idx, offset, block, n);
		offset += n;
	}
	fclose(fp);

	fp = fopen("mydisk.img", "r+b");
	fwrite(inode_bitmap, sizeof(char), (BLOCK_BITMAP_BASE - INODE_BITMAP_BASE), fp);	
	fwrite(block_bitmap, sizeof(char), (INODE_BASE - BLOCK_BITMAP_BASE), fp);
	fclose(fp);



	// FILE *fp = fopen("mydisk.img", "r+b");
	// fread(inode_bitmap, sizeof(char), (BLOCK_BITMAP_BASE - INODE_BITMAP_BASE), fp);	
	// fread(block_bitmap, sizeof(char), (INODE_BASE - BLOCK_BITMAP_BASE), fp);
	// fclose(fp);

	// current_dir = 2;
	// root_dir = 2;
	// char buf[1024] = {0};
	// mkdir("shit");
	// lsdir("/", buf);
	// print_dir(buf);




	// int i;
	// for(i = 0; i < 115; i++){
	// 	char name[20];
	// 	sprintf(name, "ex%d", i);
	// 	if(i == 111)
	// 		printf("hahah\n");
	// 	int err = make_file(name, current_dir);
	// 	if(err == -1)
	// 		printf("something wrong\n");
	// }	
	// char buf[20*SIZE_OF_BLOCK] = {'\0'};
	// lsdir(".", buf);
	// print_dir(buf);
}
#define ROOT_INODE 2
inode_t make_root(){
	inode_t root_inode_index = ROOT_INODE;
	inode_bitmap[root_inode_index] = 1;
	inode_entry root_inode;
	root_inode.size = 2 * sizeof(dir_entry);
	root_inode.type = FILE_DIR;

	dir_entry dirs[2];
	strcpy(dirs[0].filename, ".");
	dirs[0].inode = root_inode_index;
	strcpy(dirs[1].filename,"..");
	dirs[1].inode = root_inode_index;
	root_inode.index[0] = get_free_block(block_bitmap);
	write_inode(root_inode_index, &root_inode, 1);

	char block[SIZE_OF_BLOCK];
	memcpy(block, dirs, sizeof(dirs));
	write_block(root_inode.index[0], block);
	return root_inode_index;
}
int chdir(char *dir){
	inode_t res = get_file(dir, current_dir);
	if(check_file_type(current_dir) == FILE_DIR){
		current_dir = res;
		return 1;
	}else{
		return -1;
	}
}
inode_t get_file(char *path, inode_t from){
	/* the tail of path must not be '/' */
	inode_t dest = from;
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
	read_inode(file, &inode, 1);
	return inode.type;	
}
inode_t find_file(char *name, inode_t dest){
	inode_entry dir;
	char block[SIZE_OF_BLOCK];
	read_inode(dest, &dir, 1);
	int num = dir.size / SIZE_OF_BLOCK + 1;
	int len = dir.size;
	int i;
	for(i = 0; i < num; i++){
		block_t ind = index_block(i, dest, -1);
		read_block(ind, block);
		int end = len < SIZE_OF_BLOCK ?  len / sizeof(dir_entry) : SIZE_OF_BLOCK / sizeof(dir_entry); 
		dir_entry *dir_e = (dir_entry*)block;
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
void get_path_and_name(char *dir, char *path, char *name){
	int i, j = 0;
	for(i = 0; *(dir + i) != '\0'; i++)
		if(*(dir + i) == '/')	j = i + 1;
	strncpy(path, dir, j);
	path[j] = '\0';
	strcpy(name, dir + j);
}
inode_t mkdir(char *dir){
	/* must not end with '/' */
	inode_t dest;
	char name[32];
	char path[64];
	get_path_and_name(dir, path, name);
	dest = get_file(path, current_dir);
	if(check_file_type(dest) == FILE_DIR)
		return make_dir(name, dest);
	else 
		return -1;
}
int lsdir(char *dir, char *buf){
	inode_t dir_index = get_file(dir, current_dir);
	if(check_file_type(dir_index) != FILE_DIR) return -1;
	inode_entry dir_inode;
	read_inode(dir_index, &dir_inode, 1);

	int len = dir_inode.size;
	int num = len / SIZE_OF_BLOCK;
	num = len % SIZE_OF_BLOCK == 0 ? num : (num + 1);	
	int unit_num = SIZE_OF_BLOCK / sizeof(dir_entry);
	int i;
	for(i = 0; i < num; i++){
		char block[SIZE_OF_BLOCK];
		block_t ind = index_block(i, dir_index, -1);
		read_block(ind, block);
		int end = len >= SIZE_OF_BLOCK ? unit_num : len / sizeof(dir_entry); 
		dir_entry *p_dir_item = (dir_entry*)block;
		dir_entry dir_item;
		int j;
		for(j = 0; j < end; j++){
			memcpy(&dir_item, p_dir_item + j, sizeof(dir_entry));
			strcpy(buf, dir_item.filename);
			buf += strlen(dir_item.filename);
			*buf = '\0';
			buf ++;
		}
		len -= SIZE_OF_BLOCK;
	}
	return 1;
}
int rmdir(char *dir){
	inode_t dest = get_file(dir, current_dir);
	char name[32];
	char path[64];
	get_path_and_name(dir, path, name);
	printf("path: %s\nname: %s\n", path, name);
	if(check_file_type(dest) == FILE_DIR){

		char child_path[64];
		char buf[1024] = {0};
		lsdir(dir, buf);
		char *source = buf;
		char *end = buf + 1024;
		int path_len = strlen(path);

		while(*source != '\0' && source < end){

			strcpy(child_path, path);
			*(child_path + path_len) = '/';
			strcpy(child_path + path_len + 1, source);
			source += strlen(source) + 1;

			rmdir(child_path);
		}
	}else if(check_file_type(dest) == FILE_PLAIN){
		remove_file(dest);	
	}else{
		return -1;
	}

	/* update the entry of the parent directory */
	inode_t parent_index = get_file(path, current_dir);
	inode_entry parent;
	read_inode(parent_index, &parent, 1);
	int len = parent.size;
	parent.size -= sizeof(dir_entry);
	write_inode(parent_index, &parent, 1);
	int block_num = len / SIZE_OF_BLOCK + 1;

	char block[SIZE_OF_BLOCK];
	int i;
	for(i = 0; i < block_num; i ++){
		block_t block_index =  index_block(i, parent_index, -1);
		read_block(block_index, block);
		dir_entry *dir_e = (dir_entry*)block;
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
			int temp_len = block + SIZE_OF_BLOCK - (char*)(dir_e + j + 1);
			memcpy(dir_e + j, dir_e + j + 1, temp_len);
			write_block(block_index, block);
			break;
		}
	}

	return 1;
}
int remove_file(inode_t file_index){
	inode_entry file;
	read_inode(file_index, &file, 1);
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
inode_t make_dir(char *name, inode_t dest){
	inode_t inode_index = get_free_inode(inode_bitmap);
	inode_entry inode;
	inode.size = 2 * sizeof(dir_entry);
	inode.type = FILE_DIR;

	/* make . and .. entry for the new directory */
	dir_entry dirs[2];
	strcpy(dirs[0].filename, ".");
	dirs[0].inode = inode_index;
	strcpy(dirs[1].filename, "..");
	dirs[1].inode = dest;
	inode.index[0] = get_free_block(block_bitmap);
	char block[SIZE_OF_BLOCK];
	memcpy(block, dirs, inode.size);
	write_block(inode.index[0], block);
	write_inode(inode_index, &inode, 1);

	/* append the dir_entry to the back of the diretory */
	inode_entry cur_dir;
	read_inode(dest, &cur_dir, 1);
	int len = cur_dir.size;

	if(len % SIZE_OF_BLOCK == 0){
		/* add a new block for the parent directory */
		block_t index = get_free_block(block_bitmap);
		index_block(len / SIZE_OF_BLOCK, dest, index);
	}

	cur_dir.size += sizeof(dir_entry);
	int residual = (SIZE_OF_BLOCK -  cur_dir.size % SIZE_OF_BLOCK);
	if(residual < sizeof(dir_entry)){
		cur_dir.size += residual;
	}

	write_inode(dest, &cur_dir, 1);
	// int num = len % unit == 0 ? (len / unit) : (len / unit + 1);
	int seq_num = len / SIZE_OF_BLOCK;
	dir_entry dir_item;	
	strcpy(dir_item.filename, name);
	dir_item.inode = inode_index;
	/* looking for the index */
	block_t block_index = index_block(seq_num, dest, -1);
	int offset = len - SIZE_OF_BLOCK * seq_num;
	read_block(block_index, block);
	memcpy(block + offset, &dir_item, sizeof(dir_item));
	write_block(block_index, block);

	return inode_index;
}
inode_t make_file(char *name, inode_t dest){
	char block[SIZE_OF_BLOCK];	
	inode_t inode_index = get_free_inode(inode_bitmap);
	if(inode_index == -1)
		printf("something wrong\n");	
	inode_entry inode;
	inode.size = 0;
	inode.type = FILE_PLAIN;
	write_inode(inode_index, &inode, 1);

	/* append the dir_entry to the back of the diretory */
	inode_entry cur_dir;
	read_inode(dest, &cur_dir, 1);
	int len = cur_dir.size;


	cur_dir.size += sizeof(dir_entry);
	int residual = (SIZE_OF_BLOCK -  cur_dir.size % SIZE_OF_BLOCK);
	if(residual < sizeof(dir_entry)){
		cur_dir.size += residual;
	}
	
	write_inode(dest, &cur_dir, 1);

	if(len % SIZE_OF_BLOCK == 0){
		/* add a new block for the parent directory */
		block_t index = get_free_block(block_bitmap);
		index_block(len / SIZE_OF_BLOCK, dest, index);
		int t = index_block(1, dest, -1);
	}
	// int unit_num = SIZE_OF_BLOCK / sizeof(dir_entry);
	// int num = len % unit == 0 ? (len / unit) : (len / unit + 1);
	int seq_num = len / SIZE_OF_BLOCK;
	dir_entry dir_item;	
	strcpy(dir_item.filename, name);
	dir_item.inode = inode_index;
	/* looking for the index */
	block_t block_index = index_block(seq_num, dest, -1);
	int offset = len - SIZE_OF_BLOCK * seq_num;
	read_block(block_index, block);
	memcpy(block + offset, &dir_item, sizeof(dir_item));
	write_block(block_index, block);
	return inode_index;
}
block_t index_block(int seq_num, inode_t inode_index, block_t block_index){
	inode_entry inode;
	read_inode(inode_index, &inode, 1);
	block_t *index = inode.index;	
	char block[SIZE_OF_BLOCK];	
	block_t res = -1;
	int offset;
	if(seq_num >= 0 && seq_num < ONE_INDEX_BASE){
		if(block_index != -1){
			*(index + seq_num) = block_index;
			write_inode(inode_index, &inode, 1);
		}else
			res = *(index + seq_num);
	}else if(seq_num < TWO_INDEX_BASE){
		seq_num -= TWO_INDEX_BASE;
		read_block(index[12], block);
		if(block_index != -1){
			*((block_t *)block + seq_num * INDEX_SIZE) = block_index;
			write_block(index[12], block);
		}else
			res = *((block_t *)block + seq_num * INDEX_SIZE);
	}else if(seq_num < THREE_INDEX_BASE){
		seq_num -= TWO_INDEX_BASE;
		offset = seq_num / NR_INDEX;
		read_block(index[13], block);
		res = *((block_t *)block + offset * INDEX_SIZE);
		seq_num -= offset * (NR_INDEX); 
		read_block(res, block);
		if(block_index != -1){
			*((block_t *)block + offset * INDEX_SIZE) = block_index;
			write_block(index[12], block);
		}else
			res = *((block_t *)block + offset * INDEX_SIZE);
	}else{
		seq_num -= THREE_INDEX_BASE;
		offset = seq_num / (NR_INDEX * NR_INDEX);
		read_block(index[14], block);
		res = *((block_t *)block + offset * INDEX_SIZE);
		seq_num -= offset * (NR_INDEX * NR_INDEX);
		offset = seq_num / NR_INDEX;
		read_block(res, block);
		res = *((block_t *)block + offset * INDEX_SIZE);
		seq_num -= offset * NR_INDEX;
		read_block(res, block);
		if(block_index != -1){
			*((block_t *)block + offset * INDEX_SIZE) = block_index;
			write_block(index[12], block);
		}else
			res = *((block_t *)block + offset * INDEX_SIZE);
	}
	if(block_index == -1) return res;
	else return 1;
}
void write_inode(inode_t index, inode_entry *data, int count){
	long int offset = INODE_BASE + sizeof(inode_entry) * index;
	FILE *fp = fopen("mydisk.img", "r+b");
	if(fp == 0) printf("error\n");
	fseek(fp, offset, SEEK_SET);
	fwrite(data, sizeof(inode_entry), count, fp);
	fclose(fp);
}
void read_inode(inode_t index, inode_entry *data, int count){
	long int offset = INODE_BASE + sizeof(inode_entry) * index;
	FILE *fp = fopen("mydisk.img", "r+b");
	fseek(fp, offset, SEEK_SET);
	fread(data, sizeof(inode_entry), count, fp);
	fclose(fp);
}
void read_block(block_t index, void *data){
	long int offset = BLOCK_BASE + SIZE_OF_BLOCK * index;
	FILE *fp = fopen("mydisk.img", "r+b");
	fseek(fp, offset, SEEK_SET); 
	fread(data, sizeof(char), SIZE_OF_BLOCK, fp);
	fclose(fp);
}
void write_block(block_t index, void *data){
	printf("writing block oh yeah\n");
	long int offset = BLOCK_BASE + SIZE_OF_BLOCK * index;
	FILE *fp = fopen("mydisk.img", "r+b");
	fseek(fp, offset, SEEK_SET);
	fwrite(data, sizeof(char), SIZE_OF_BLOCK, fp);
	fclose(fp);
}
inode_t get_free_inode(char *inode_bitmap){
	int i;
	for(i = 0; i < NR_INODE; i++){
		if(inode_bitmap[i] == 0){
			inode_bitmap[i] = 1;
			return i;
		}
	}
	return -1;
}

block_t get_free_block(char *block_bitmap){
	int i;
	for(i = 0; i < NR_BLOCK; i++){
		if(block_bitmap[i] == 0){
			block_bitmap[i] = 1;
			return i;
		}
	}
	return -1;
}

char blk[SIZE_OF_BLOCK];

static void file_write(int file_inode, int offset, char *buf, int len){

	/* dev_id as fd, it is ok because the real dev_id is stored in s_fde */

	inode_entry inode_e;
	read_inode(file_inode, &inode_e, 1);

	int delta_len = 0;
	if(offset + len > inode_e.size){
		printf("\n\nwriting overflow!\n\n");
		delta_len = offset + len - inode_e.size + (inode_e.size % SIZE_OF_BLOCK);
		inode_e.size = offset + len;
		len -= delta_len;
	}

	int seq_num = offset / SIZE_OF_BLOCK;
	offset = offset - seq_num * SIZE_OF_BLOCK;

	while(len != 0){
		block_t blk_idx = index_block(seq_num, file_inode, -1);
		read_block(blk_idx, blk);
		int write_size = (len < SIZE_OF_BLOCK ? len : SIZE_OF_BLOCK) - offset;
		memcpy(blk + offset, buf, write_size);
		offset = 0;
		len -= write_size;
		buf += write_size;
		seq_num ++;
	}
	write_inode(file_inode, &inode_e, 1);

	while(delta_len != 0){
		block_t new_blk = get_free_block(block_bitmap);
		index_block(seq_num, file_inode, new_blk);
		// printf("after index_block, new_blk: %d. after search: %d", new_blk, index_block(seq_num, file_inode, -1));
		int write_size = (delta_len < SIZE_OF_BLOCK ? delta_len : SIZE_OF_BLOCK);
		memcpy(blk, buf, write_size);
		write_block(new_blk, blk);
		delta_len -= write_size;
		buf += write_size;
		seq_num ++;
	}

}

static void file_read(int file_inode, int offset, char *buf, int len){

	/* using msg->dest to distinguish between the file_system & origin file */
	/* dev_id is the id of ramdisk, namely, file_system */
	/* in reading file_system, you don't specify offset */
	/* input : offset, len, buf, inode */
	inode_entry inode_e;
	read_inode(file_inode, &inode_e, 1);
	if(offset + len > inode_e.size){
		printf("\n\nreading overflow!\n\n");
	}

	int seq_num = offset / SIZE_OF_BLOCK;
	offset = offset - seq_num * SIZE_OF_BLOCK;

	while(len != 0){
		block_t blk_idx = index_block(seq_num, file_inode, -1);
		read_block(blk_idx, blk);
		int read_size = (len < SIZE_OF_BLOCK ? len : SIZE_OF_BLOCK) - offset;
		memcpy(buf, blk + offset, read_size);
		offset = 0;
		len -= read_size;
		buf += read_size;
		seq_num ++;
	}

}