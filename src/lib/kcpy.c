#include "kernel.h"
#include "string.h"
static void* kernel_va(PCB* pcb, void *src);
void copy_from_kernel(PCB* pcb, void* dest, void* src, int len){
	src = kernel_va(pcb, src);
	memcpy(dest, src, len);
}

void copy_from_kernel_mine(PCB* pcb, void* dest, void* src, int len){
	dest = kernel_va(pcb, dest);
	memcpy(dest, src, len);
}

void set_from_kernel_mine(PCB* pcb, void* dest, uint8_t data, int len){
	dest = kernel_va(pcb, dest);
	memset(dest, data, len);
}

void copy_to_kernel(PCB* pcb, void* dest, void* src, int len){
	/* when in kernel space */
	/* dest is in kernel space, src is in user space */
	src = kernel_va(pcb, src);
	memcpy(dest, src, len);
}

void strcpy_to_kernel(PCB* pcb, char* dest, char* src){
	src = kernel_va(pcb, src);
	strcpy(dest, src);
}

void strcpy_from_kernel(PCB* pcb, char* dest, char* src){
	src = kernel_va(pcb, src);
	strcpy(dest, src);
}

static void* kernel_va(PCB* pcb, void *src){
	uint32_t va = (uint32_t)src;
	CR3 *cr3 = pcb->cr3;
	PDE *pde = (PDE*) pa_to_va(cr3->page_directory_base << 12);
	pde += (va >> 22);
	PTE *pte = (PTE*) pa_to_va(pde->page_frame << 12);
	pte += ((va >> 12) & 0x3ff);
	va = (pte->page_frame << 12) + (va & 0xfff);
	return (void*)pa_to_va(va);
}
