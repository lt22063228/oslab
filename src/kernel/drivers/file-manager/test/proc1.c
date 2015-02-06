#define SYS_puts 3
#define SYS_fork 4
#define SYS_exec 5
#define	SYS_exit 6 
#define SYS_getpid 7
#define SYS_waitpid 8
#define SYS_gets 9
// volatile int x = 0;
int syscall(int id,...);
// int main(){
// 	volatile int t ;
// 	int filename = 1;
// 	char *args = "gcc -O2 -o hello hello.c";
// 	t = syscall(SYS_fork, filename, args);
// 	if(t == 0){
// 		t ++;
// 	}else{
// 		t ++;
// 	}
// 	while(1){
// 		if(x % 10000000 == 0){
// 		}
// 		x ++;
// 	}
// 	return 0;
// }
int __attribute__((__noinline__))
syscall(int id, ...) {
	int ret;
	int *args = &id;
	asm volatile("int $0x80": "=a"(ret) : "a"(args[0]), "b"(args[1]), "c"(args[2]), "d"(args[3]));
	return ret;
}

// static void read_line(char *cmd);
static unsigned int fork();
// static void waitpid(int pid);
// static int exec(int filename, char *cmd);
// static void shell(void);
static void test_fork();
int main(char *args){
	test_fork();
	return 1;
}
static void test_fork(){
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
// static void shell(){
// 	char cmd[256];
// 	while(1){
// 		read_line(cmd);
// 		int filename = cmd[0] - '0';
// 		int pid = fork();
// 		if( pid == 0) {
// 			exec(filename, cmd);
// 		}else {
// 			waitpid(pid);
// 		}
// 	}
// }

// static void read_line(char *cmd){
// 	syscall(SYS_gets, cmd);	
// }
// static void waitpid(int pid){
// 	syscall(SYS_waitpid, pid);
// }
static unsigned int fork(){
	return syscall(SYS_fork);	
} 
// static int exec(int filename, char *cmd){
// 	return syscall(SYS_exec, filename, cmd);
// }

