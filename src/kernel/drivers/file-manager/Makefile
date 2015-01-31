cc = gcc
LD = ld
LDFLAGS = -melf_i386
CFLAGS  = -m32 -static -ggdb -MD -Wall -Werror -I./include -O0 \
		 -fno-builtin -fno-stack-protector -fno-omit-frame-pointer
CFILES  = $(shell find ./test -name "*.c")
OBJS    = $(CFILES:.c=.o) 

test.img: $(OBJS)
	$(LD) $(LDFLAGS) -e main -Ttext 0x08048000 -o test.img $(OBJS)
	readelf -a test.img > elf.txt
	sh dump-file.sh test.img

-include $(OBJS:.o=.d)

clean:
	rm -f test.img $(OBJS) $(OBJS:.o=.d)
	rm a.txt
