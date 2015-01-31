#define SYS_puts 3
#define SYS_fork 4
volatile int x = 0;
int syscall(int id,...);
int main(){
	volatile int t ;
	t = syscall(SYS_fork);
	if(t == 0){
		t ++;
	}else{
		t ++;
	}
	while(1){
		if(x % 10000000 == 0){
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
