#include "kernel.h"
#include "hal.h"
#include "memory.h"
#include "x86/memory.h"

#define NR_PROC 10
#define NR_KERNEL_THREAD 10
#define NR_PAGE_PER_PROC 1024
#define NR_PTABLE 128

pid_t MM;
/* 128M memory, 4M for each user process, support 22 user process from [40M,128M) */
static CR3 cr3[NR_PROC];
static PDE pdir[NR_PROC][NR_PDE] align_to_page;
/* total PHY_MEM / PAGE_SIZE PTE, one for each */
static PTE ptable[NR_PTABLE][NR_PTE] align_to_page;

typedef struct PTABLES{
	PTE *pt;
	ListHead list; 
} Ptable;
Ptable pts[NR_PTABLE];

static int pframe[NR_PROC] = {0};
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
	for(pt_idx = 0; pt_idx < NR_PTABLE; pt_idx++){
		pts[pt_idx].pt = ptable[pt_idx];
		list_init(&pts[pt_idx].list);
		list_add_before(&pt_free, &pts[pt_idx].list);
		for(pte_idx = 0; pte_idx < NR_PTE; pte_idx++){
			make_invalid_pte(&ptable[pt_idx][pte_idx]);
		}
	}
	PCB *p = create_kthread( mmd );
	MM = p->pid;
	wakeup(p);
}
static void map_kernel(pid_t req_pid);
static void
mmd(void){
	Msg m;
	while(1){
		receive( ANY, &m );
		pid_t req_pid = m.req_pid;
		uint32_t va;
		uint32_t puser_idx = req_pid - NR_KERNEL_THREAD;
		size_t memsz;
		int num_page;
		switch( m.type ){
			case MAP_KERNEL:
				/* argument : req_pid */
				/* return : none */
				map_kernel(req_pid);
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
				
				
				int num;
				/* store the start of the allocated physical address,
				   assuming allocated address are sequantial */
				int pa = (req_pid << 22) + (pframe[puser_idx] << 12);
				for(num = 0; num < num_page; num++){
					va = m.offset + (num << 12);
					/* follow the right track */
					PDE *pde = pa_to_va(&((PDE*)(cr3[puser_idx].page_directory_base << 12))[va >> 22]);
					PTE *pt;	
					if(pde->present == 0){
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
						make_pde(pde, va_to_pa(pt));
					}else{
						pt = (PTE *)(pde->page_frame << 12);
					}
					PTE *pte = &pt[(va >> 12) & 0x3ff];

					if(pte->present == 0){
						/* allocate a pframe */
						if(pframe[puser_idx] == NR_PAGE_PER_PROC){
							assert(0);
						}
						/*pa is page_frame's physical address */
						make_pte(pte, 
							(void*)(req_pid << 22) + (pframe[puser_idx] << 12));
						pframe[puser_idx] ++;
					}
				}
				/* must return virtual address w.r.t kernel,
				   all thread other than memory manage recognize
				   physical address */
				m.ret = (int)pa_to_va(pa);
				m.dest = m.src;
				m.src = MM;
				send( m.dest, &m );
				break;
			case NEW_PCB:
				m.dest = m.src;
				m.ret = (int)&cr3[puser_idx];
				m.src = MM;
				send( m.dest, &m );
				break;
			case FORK:
				map_kernel(req_pid);
				CR3 *pcr3 = (CR3*)m.buf;
				CR3 *ccr3 = &cr3[puser_idx];
				PDE *ppde = (PDE*) pa_to_va((pcr3->page_directory_base << 12));
				PDE *cpde = (PDE*) pa_to_va((ccr3->page_directory_base << 12));
				int i, j;

				/* don't map kernel again */
				for(i = 0; i < NR_PDE; i++){
					if(ppde->present == 1 && cpde->present == 0){
						/* allocate a ptable for child */
						ListHead *p = pt_free.next;
						PTE *pt = list_entry(p, Ptable, list)->pt;
						list_del(p);
						list_add_before(&pt_used, p);
						/* only accept physical address */
						make_pde(cpde, va_to_pa(pt));

						PTE *cpte = (PTE*)pa_to_va(( cpde->page_frame << 12 ));
						PTE *ppte = (PTE*)pa_to_va(( ppde->page_frame << 12 ));
						for(j = 0; j < NR_PTE; j++){
							if(ppte->present == 1 && cpte->present == 0){
								void *cpa = (void*)(m.req_pid << 22) + (pframe[puser_idx] << 12);
								void *ppa = (void*)(ppte->page_frame << 12);
								make_pte(cpte, cpa);
								pframe[puser_idx] ++;
								/* copy the page_frame */
								memcpy(pa_to_va(cpa), pa_to_va(ppa), PAGE_SIZE);
							}
							ppte ++;
							cpte ++;
						}
					}
					ppde ++;
					cpde ++;
				}
				m.dest = m.src;
				m.src = MM;
				send( m.dest, &m );
				break;
			default:
				assert(0);
				break;
		}
	}
}
static void map_kernel(pid_t req_pid){
	int puser_idx = req_pid - NR_KERNEL_THREAD;
	int pdir_idx;
	PTE *ptable = (PTE *)va_to_pa(get_kptable());
	/* map only 40M space for kernel */
	for (pdir_idx = 0; pdir_idx < NR_KERNEL_THREAD; pdir_idx ++) {
		/* store the address of kernel page table
		 * because pte has been initialized in the
		 * kernel, we are not bother to do so */
		make_pde(&pdir[puser_idx][pdir_idx], ptable);
		make_pde(&pdir[puser_idx][pdir_idx + KOFFSET / PD_SIZE], ptable);
		ptable += NR_PTE;
	}
}