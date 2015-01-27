#ifndef __X86_IO_H__
#define __X86_IO_H__

#include "common.h"

static inline uint8_t
in_byte(uint16_t port) {
	uint8_t data;
	asm volatile("in %1, %0" : "=a"(data) : "d"(port));
	return data;
}

static inline uint32_t
in_long(uint16_t port) {
	uint32_t data;
	asm volatile("in %1, %0" : "=a"(data) : "d"(port));
	return data;
}

static inline void
out_byte(uint16_t port, uint8_t data) {
	asm volatile("out %%al, %%dx" : : "a"(data), "d"(port));
}

static inline void
out_long(uint16_t port, uint32_t data) {
	asm volatile("out %%eax, %%dx" : : "a"(data), "d"(port));
}

/* Structure of a ELF binary header */
struct ELFHeader {
	unsigned int   magic;
	unsigned char  elf[12];
	unsigned short type;
	unsigned short machine;
	unsigned int   version;
	unsigned int   entry;
	unsigned int   phoff;
	unsigned int   shoff;
	unsigned int   flags;
	unsigned short ehsize;
	unsigned short phentsize;
	unsigned short phnum;
	unsigned short shentsize;
	unsigned short shnum;
	unsigned short shstrndx;
};

/* Structure of program header inside ELF binary */
struct ProgramHeader {
	unsigned int type;
	unsigned int off;
	unsigned int vaddr;
	unsigned int paddr;
	unsigned int filesz;
	unsigned int memsz;
	unsigned int flags;
	unsigned int align;
};
#endif
