#define SYS_puts 3
#define SYS_fork 4
#define SYS_exec 5
#define	SYS_exit 6 
#define SYS_getpid 7
#define SYS_waitpid 8
#define SYS_gets 9
#define SYS_cat 10
#define SYS_open 11
#define SYS_read 12
#define SYS_write 13
#define SYS_close 14
#define SYS_lseek 15
#define SYS_echo 16
#define SYS_makefile 17
#define SYS_listdir 18
#define SYS_mkdir	19
#define SYS_chdir	20
#define SYS_rmdir	21
int syscall(int id,...);
int __attribute__((__noinline__))
syscall(int id, ...) {
	int ret;
	int *args = &id;
	asm volatile("int $0x80": "=a"(ret) : "a"(args[0]), "b"(args[1]), "c"(args[2]), "d"(args[3]));
	return ret;
}

static void read_line(char *cmd);
static unsigned int fork();
static void waitpid(int pid);
static int exec(int filename, char *cmd);
static void cat(int filename);
static void shell(void);
static void test_exec();
static void test_fork();
static void test_cat();
static void simple();

static void list_dir();
static void make_file(char *name);
static void mkdir(char *name);
static void chdir(char *name);
static void rm(char *name);
static int read(int fd, void *buf, int len);
static int write(int fd, void *buf, int len);
static int open(char *filename);
static int close(int fd);
static int lseek(int fd, int offset, int whence);
static void echo(void *buf);
int main(char *args){
	shell();
	test_cat();
	test_cat();
	test_exec();
	test_fork();
	simple();
	return 1;
}
static void simple(){
	return;
	volatile int x = 0;
	while(1){
		x ++;
	}
}
static void test_fork(){
	return;
	volatile int t = fork();

	if(t == 0){
		/*child*/
		while(1){
			t --;
		}
	}else{
		/*parent*/
		while(1){
			t ++;
		}
	}
}
static void test_cat(){
	return;
	volatile int x = 0;
	cat(5);
	while(1){
		x ++;
	}
}
#define NR_CMD 13
#define CMD_ERROR -1
#define CMD_FORK 0
#define CMD_CAT 1
#define CMD_OPEN 2
#define CMD_CLOSE 3
#define CMD_READ 4
#define CMD_WRITE 5
#define CMD_LSEEK 6
#define CMD_ECHO 7
#define CMD_MAKEFILE 8
#define CMD_LISTDIR 9
#define CMD_MKDIR	10
#define CMD_CHDIR	11
#define CMD_RM		12
char builtin[NR_CMD][16] = {"exec","cat","open","close","read","write","lseek","echo", "make_file", "list", "mkdir", "cd", "rm"};
// 0 for no parameter, 1 for integer, 2 for char array
char arg_type[NR_CMD][8] = {"100", "100", "200", "100", "121", "121",  "111",  "200",  "200",    	  "200", "200",	  "200", "200"};
static int parse(char *cmd, void *arg){
	int i;
	for(i = 0; i < NR_CMD; i++){
		char *target = builtin[i];
		int is_cmd = -1;
		int j;
		for(j = 0; *(cmd + j) != '\0' && *(cmd + j) != ' '; j++){
			if(*(target + j) != *(cmd + j)){
				is_cmd = -1;
				break;
			}
			is_cmd = 1;
		}	
		if(is_cmd == 1){
			if(arg_type[i][0] == '1'){
				int *int_arg = (int*)arg;
				while(*(cmd + j) == ' ') j++;
				*int_arg = 0;
				for(;*(cmd + j) != ' ' && *(cmd + j) != '\0'; j++){
					int digit = (*(cmd + j) - '0');
					if(is_cmd == 1 && (digit < 0 || digit > 10) ) return -1;
					*int_arg += *int_arg * 10 + digit;
				}
			}else if(arg_type[i][0] == '2'){
				char *char_arg = (char*)arg;
				while(*(cmd + j) == ' ') j++;
				for(;*(cmd + j) != ' ' && *(cmd + j) != '\0'; j++){
					*char_arg = *(cmd + j);
					char_arg ++;
				}
				// *char_arg ++ = '\n';
				*char_arg = '\0';
			}else{
				return -2;	
			}
			return i;
		}
	}
	return -1;
}
static void shell(){
	char cmd[64];
	char arg[64];
	char wr_buf[64] = {"writing to file success!\n"};
	char file_name[10] = {"test_file"};
	char buf[64];
	int num;
	int fd = open(file_name);
	write(fd, wr_buf, 64);
	lseek(fd, 0, 0);
	read(fd, buf, 64);
	write(1, buf, 64);
	lseek(fd, 0, 0);
	close(fd);
	while(1){
		read_line(cmd);
		int cmd_type = parse(cmd, (void*)arg);
		switch(cmd_type){
			case CMD_ERROR:
				break;
			case CMD_FORK:
				num = *((int*)arg);
				cmd_type = -1;
				int pid = fork();
				if( pid == 0) {
					exec(num, cmd);
				}else {
					waitpid(pid);
				}
				break;
			case CMD_CAT:
				num = *((int*)arg);
				cat(num);
				break;
			case CMD_ECHO:
				echo((void *)buf);
				break;
			case CMD_MAKEFILE:
				make_file((char*)arg);
				break;
			case CMD_LISTDIR:
				list_dir();
				break;
			case CMD_MKDIR:
				mkdir((char*)arg);
				break;
			case CMD_CHDIR:
				chdir((char*)arg);
				break;
			case CMD_RM:
				rm((char*)arg);
				break;
			default:
				echo("default switch in proc1.c\n");
				break;
		}
	}
}
static void rm(char *name){
	syscall(SYS_rmdir, name);
}
static void chdir(char *name){
	syscall(SYS_chdir, name);
}
static void list_dir(){
	syscall(SYS_listdir);
}
static void mkdir(char *name){
	syscall(SYS_mkdir, name);
}
static void make_file(char *name){
	syscall(SYS_makefile, name);
}
static void read_line(char *cmd){
	syscall(SYS_gets, cmd);	
}
static void waitpid(int pid){
	syscall(SYS_waitpid, pid);
}
static unsigned int fork(){
	return syscall(SYS_fork);	
} 
static int exec(int filename, char *cmd){
	return syscall(SYS_exec, filename, cmd);
}
static void cat(int filename){
	syscall(SYS_cat, filename);
}
static int read(int fd, void *buf, int len){
	return syscall(SYS_read, fd, buf, len);
}
static int write(int fd, void *buf, int len){
	return syscall(SYS_write, fd, buf, len);
}
static int open(char *filename){
	return syscall(SYS_open, filename);
}
static int close(int fd){
	return syscall(SYS_close, fd);
}
static int lseek(int fd, int offset, int whence){
	return syscall(SYS_lseek, fd, whence);
}
static void echo(void *buf){
	syscall(SYS_echo, buf);	
}
static void test_exec(){
	return;
	exec(0, "nothing");
}
