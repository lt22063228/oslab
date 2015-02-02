// #define SYS_puts 3
// #define SYS_fork 4
// #define SYS_exec 5
// volatile int x = 0;
// int syscall(int id,...);
// int main(){
// 	volatile int t ;
// 	int filename = 1;
// 	char *args = "gcc -O2 -o hello hello.c";
// 	t = syscall(SYS_exec, filename, args);
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
// int __attribute__((__noinline__))
// syscall(int id, ...) {
// 	int ret;
// 	int *args = &id;
// 	asm volatile("int $0x80": "=a"(ret) : "a"(args[0]), "b"(args[1]), "c"(args[2]), "d"(args[3]));
// 	return ret;
// }

volatile int x= 0;
int main(char *args){
	while(1){
		x ++;
	}
}