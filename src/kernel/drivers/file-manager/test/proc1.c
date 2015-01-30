#define SYS_puts 3
volatile int x = 0;
int syscall(int id,...);
int main(){
	while(1){
		if(x % 10000000 == 0){
			volatile int t = syscall(SYS_puts, 1, 2, 3);
			t ++;
		}
		x ++;
	}
	return 0;
}
int __attribute__((__noinline__))
syscall(int id, ...) {
	int ret;
	int *args = &id;
	asm volatile("int $0x80": "=a"(ret) : "a"(args[0]), "b"(args[1]), "c"(args[2]), "d"(args[3]));
	return ret;
}
