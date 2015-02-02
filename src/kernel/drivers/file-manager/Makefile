VAR = "default"
cc = gcc
LD = ld
LDFLAGS = -melf_i386
CFLAGS  = -Wno-main -m32 -static -ggdb -MD -Wall -Werror -I./include -O0 \
		 -fno-builtin -fno-stack-protector -fno-omit-frame-pointer
CFILES  = $(shell find ./test -name "*.c")
OBJS    = $(CFILES:.c=.o) 

$(VAR).img: $(OBJS)
	$(LD) $(LDFLAGS) -e main -Ttext 0x08048000 -o $(VAR).img $(OBJS)
	readelf -a $(VAR).img > elf.txt
	sh dump-file.sh $(VAR).img
	cp $(VAR).img ../../../../$(VAR).img

-include $(OBJS:.o=.d)

clean:
	rm -f $(VAR).img $(OBJS) $(OBJS:.o=.d)
	rm a.txt
	rm ../../../../$(VAR).img
