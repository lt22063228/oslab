#include "kernel.h"
#include "hal.h"
#include "memory.h"
#include "x86/memory.h"

#define NR_PROC 22
#define NR_KERNEL_THREAD 10
#define NR_PAGE_PER_PROC 1024

pid_t MM;
/* 128M memory, 4M for each user process, support 22 user process from [40M,128M) */
static CR3 cr3[NR_PROC];
static PDE pdir[NR_PROC][NR_PDE] align_to_page;
/* total PHY_MEM / PAGE_SIZE PTE, one for each */
static PTE ptable[PHY_MEM / PAGE_SIZE][NR_PTE] align_to_page;

typedef struct PTABLES{
	PTE *pt;
	ListHead list; 
} Ptable;
Ptable pts[PHY_MEM / PAGE_SIZE];

static bool pframe[NR_PROC] = {0};
static void mmd(void);
/* every PDE should point to the same page table
 * this is due to the 4M space restriction,
 * Assuming process occupy sequential memory address for 4M */
ListHead pt_free, pt_used;
void init_mm(void) {
	int user_idx, pde_idx;
	for(user_idx = 0; user_idx < NR_PROC; user_idx++){
		cr3[user_idx].val = 0;
		cr3[user_idx].page_directory_base = (uint32_t)va_to_pa(&pdir[user_idx])>>12;
		for(pde_idx = 0; pde_idx < NR_PDE; pde_idx++){
			make_invalid_pde(&pdir[user_idx][pde_idx]);
		}
	}
	int pt_idx, pte_idx;
	list_init(&pt_free);
	list_init(&pt_used);
	for(pt_idx = 0; pt_idx < PHY_MEM / PAGE_SIZE; pt_idx++){
		pts[pt_idx].pt = ptable[pt_idx];
		list_init(&pts[pt_idx].list);
		list_add_before(&pt_free, &pts[pt_idx].list);
		for(pte_idx = 0; pte_idx < NR_PTE; pte_idx++){
			make_invalid_pte(&ptable[pt_idx][pte_idx]);
		}
	}
	PCB *p = create_kthread( mmd );
	MM = p->pid; wakeup(p);
}
static void
mmd(void){
	Msg m;
	while(1){
		receive( ANY, &m );
		pid_t req_pid;
		uint32_t va;
		uint32_t puser_idx, pdir_idx;
		size_t memsz;
		int num_page;
		switch( m.type ){
			case MAP_KERNEL:
				req_pid = m.req_pid;
				puser_idx = req_pid - NR_KERNEL_THREAD;
				PTE *ptable = (PTE *)va_to_pa(get_kptable());
				for (pdir_idx = 0; pdir_idx < PHY_MEM / PD_SIZE; pdir_idx ++) {
					/* store the address of kernel page table
					 * because pte has been initialized in the
					 * kernel, we are not bother to do so */
					make_pde(&pdir[puser_idx][pdir_idx], ptable);
					make_pde(&pdir[puser_idx][pdir_idx + KOFFSET / PD_SIZE], ptable);
				}
				m.dest = m.src;
				m.src = MM;
				send( m.dest, &m );
				break;
			case NEW_PAGE:
				/* static allocation, which means the address is fixed */
				req_pid = m.req_pid;
				va = m.offset;
				memsz = m.len;
				num_page = memsz % PAGE_SIZE == 0 ? memsz / PAGE_SIZE : memsz / PAGE_SIZE + 1;
				puser_idx = req_pid - NR_KERNEL_THREAD;
				
				/* this step is duplicate right now */
				/*
				uint32_t pde = ((uint32_t *)(cr3[puser_idx].page_base_directory << 12))[va >> 22];
				*/	
				/* because pdirectory is mapped directly */
				int num;
				/* store the start of the allocated physical address,
				   assuming allocated address are sequantial */
				int pa = (4*req_pid << 22) + (pframe[puser_idx] << 12);
				for(num = 0; num < num_page; num++){
					va = m.offset + (num << 12);
				
					PDE pde = pdir[puser_idx][va >> 22];	

					PTE *pt;	
					if(pde.present == 0){
						/* allocate a ptable */
						if(list_empty(&pt_free) == true){
							assert(0);
						}	
						/* there is no race condition */
						ListHead *p = pt_free.next;
						pt = list_entry(p, Ptable, list)->pt;
						list_del(p);
						list_add_before(&pt_used, p);
						/* only accept physical address */
						make_pde(&pde, va_to_pa(pt));
					}else{
						pt = (PTE *)(pde.page_frame << 12);
					}
					PTE pte = pt[(va >> 12) & 0x3ff];

					if(pte.present == 0){
						/* allocate a pframe */
						if(pframe[puser_idx] == NR_PAGE_PER_PROC){
							assert(0);
						}
						/*pa is page_frame's physical address */
						make_pte(&pte, 
							(void*)(4*req_pid << 22) + (pframe[puser_idx] << 12));
						pframe[puser_idx] ++;
					}
				}
				/*	
				uint32_t pde = ((uint32_t *)(cr3 & ~0xfff))[va >> 22];
				uint32_t pte = ((uint32_t *)(pde & ~0xfff))[(va >> 12) & 0x3ff];
				uint32_t pa = (pte & ~0xfff) | (va & 0xfff);
				*/
				/* must return virtual address w.r.t kernel,
				   all thread other than memory manage recognize
				   physical address */
				m.ret = (int)pa_to_va(pa);
				m.dest = m.src;
				m.src = MM;
				send( m.dest, &m );
				break;
			case NEW_PCB:
				puser_idx = m.req_pid - NR_KERNEL_THREAD;
				m.dest = m.src;
				m.ret = (int)&cr3[puser_idx];
				m.src = MM;
				send( m.dest, &m );
				break;
			default: assert(0);
		}
	}
}
