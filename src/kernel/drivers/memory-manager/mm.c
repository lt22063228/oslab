#include "kernel.h"
#include "hal.h"
#include "memory.h"
#include "x86/memory.h"

#define NR_PROC 10
#define NR_KERNEL_THREAD 10
#define NR_PAGE_PER_PROC 1024
#define NR_PTABLE 128
#define NR_PFRAME (22 * 1024)

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
ListHead pt_free;
ListHead pt_used[NR_PROC];

typedef struct PFRAME
{
	void *frame;
	ListHead list;
} Pframe;
Pframe pfs[NR_PFRAME];
ListHead pf_free;
static int pframe[NR_PROC] = {0};
ListHead pf_used[NR_PROC];


static void mmd(void);
static void new_page(Msg *msg);
static void fork(Msg *msg);
static void reclaim(Msg *msg);
static void map_kernel(Msg *msg);
static void new_pcb(Msg *msg);

/* every PDE should point to the same page table
 * this is due to the 4M space restriction,
 * Assuming process occupy sequential memory address for 4M */
void init_mm(void) {
	/* per user process setting */
	int user_idx, pde_idx;
	for(user_idx = 0; user_idx < NR_PROC; user_idx++){
		list_init(&pf_used[user_idx]);
		list_init(&pt_used[user_idx]);
		cr3[user_idx].val = 0;
		cr3[user_idx].page_directory_base = (uint32_t)va_to_pa(&pdir[user_idx])>>12;
		for(pde_idx = 0; pde_idx < NR_PDE; pde_idx++){
			make_invalid_pde(&pdir[user_idx][pde_idx]);
		}
	}

	/* page table setting */
	int i, j;
	list_init(&pt_free);
	for(i = 0; i < NR_PTABLE; i ++){
		pts[i].pt = ptable[i]; // pt is virtual address w.s.t kernel.
		list_init(&pts[i].list);
		list_add_before(&pt_free, &pts[i].list);
		for(j = 0; j < NR_PTE; j++){
			make_invalid_pte(&ptable[i][j]);
		}
	}

	/* page frame setting */
	list_init(&pf_free);
	void *pa = (void*)(40 << 20);
	for(i = 0; i < NR_PFRAME; i++ ){
		pfs[i].frame = 	pa;	//frame is physical address .
		list_init(&pfs[i].list);
		list_add_before(&pf_free, &pfs[i].list);
		pa = pa + 4096;
	}

	PCB *p = create_kthread( mmd );
	MM = p->pid;
	wakeup(p);
}
static void mmd(void){
	Msg msg;
	while(1){
		receive( ANY, &msg );
		switch( msg.type ){
			case MAP_KERNEL:
				/* argument : req_pid */
				/* return : none */
				map_kernel(&msg);
				break;
			case NEW_PAGE:
				new_page(&msg);	
				break;
			case NEW_PCB:
				new_pcb(&msg);
				break;
			case FORK:
				fork(&msg);	
				break;
			case EXEC:
				/* input : req_pid */
				/* output: invalidate address space */
				reclaim(&msg);	
				break;
			case EXIT:
				reclaim(&msg);
				break;
			default:
				assert(0);
				break;
		}
	}
}

static void new_pcb(Msg *msg){
	uint32_t puser_idx = msg->req_pid - NR_KERNEL_THREAD;
	msg->dest = msg->src;
	msg->ret = (int)&cr3[puser_idx];
	msg->src = MM;
	send(msg->dest, msg);
}

static void map_kernel_assist(pid_t req_pid){
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
static void map_kernel(Msg *msg){
	uint32_t req_pid = msg->req_pid;
	map_kernel_assist(req_pid);
	msg->dest = msg->src;
	msg->src = MM;
	send(msg->dest, msg);
}

static void new_page(Msg *msg){
	/* static allocation, which means the address is fixed */
	pid_t req_pid = msg->req_pid;
	uint32_t puser_idx = req_pid - NR_KERNEL_THREAD;
	uint32_t va = msg->offset;
	uint32_t memsz = msg->len;
	uint32_t num_page = memsz % PAGE_SIZE == 0 ? memsz / PAGE_SIZE : memsz / PAGE_SIZE + 1;
	
	// int pa = (req_pid << 22) + (pframe[puser_idx] << 12);

	/* the allocated address must be sequential, in order for kernel to used */
	int num;
	for(num = 0; num < num_page; num++){
		va = msg->offset + (num << 12);
		/* follow the right track */
		PDE *pde = (PDE*) pa_to_va(cr3[puser_idx].page_directory_base << 12);
		pde += (va >> 22);
		if(pde->present == 0){
			/* allocate a ptable */
			if(list_empty(&pt_free) == true){
				assert(0);
			}	
			/* there is no race condition */
			ListHead *p = pt_free.next;
			list_del(p);
			list_add_before(&pt_used[puser_idx], p);
			/* only accept physical address */
			PTE* pt = list_entry(p, Ptable, list)->pt;
			make_pde(pde, va_to_pa(pt));
		}

		PTE *pte = (PTE*) pa_to_va(pde->page_frame << 12);
		pte += ((va >> 12) & 0x3ff);
		if(pte->present == 0){
			/* allocate a pframe */
			if(pframe[puser_idx] == NR_PAGE_PER_PROC){
				assert(0);
			}
			/*pa is page_frame's physical address */
			ListHead *p = pf_free.next;
			list_del(p);
			list_add_before(&pf_used[puser_idx], p);
			/* only accept physical address */
			void* frame = list_entry(p, Pframe, list)->frame;
			make_pte(pte, frame);
			// make_pte(pte,(void*)(req_pid << 22) + (pframe[puser_idx] << 12));
			pframe[puser_idx] ++;
		}else{
			assert(0);
		}
	}
	/* must return virtual address w.r.t kernel,
	   all thread other than memory manage recognize
	   physical address */
	msg->dest = msg->src;
	msg->src = MM;
	send(msg->dest, msg);
}

static void fork(Msg *msg){
	pid_t req_pid = msg->req_pid;
	uint32_t puser_idx = req_pid - NR_KERNEL_THREAD;
	map_kernel_assist(req_pid);
	CR3 *pcr3 = (CR3*)msg->buf;
	CR3 *ccr3 = &cr3[puser_idx];
	PDE *ppde = (PDE*) pa_to_va((pcr3->page_directory_base << 12));
	PDE *cpde = (PDE*) pa_to_va((ccr3->page_directory_base << 12));
	int i, j;
	/* don't map kernel again */
	/* if it is kernel address, it is make valid in map_kernel_assist() */
	for(i = 0; i < NR_PDE; i++){
		if(ppde->present == 1 && cpde->present == 0){
			/* allocate a ptable for child */
			ListHead *p = pt_free.next;
			list_del(p);
			list_add_before(&pt_used[puser_idx], p);
			/* only accept physical address */
			PTE *pt = list_entry(p, Ptable, list)->pt;
			make_pde(cpde, va_to_pa(pt));
			PTE *cpte = (PTE*)pa_to_va(( cpde->page_frame << 12 ));
			PTE *ppte = (PTE*)pa_to_va(( ppde->page_frame << 12 ));
			for(j = 0; j < NR_PTE; j++){
				if(ppte->present == 1 && cpte->present == 0){
					p = pf_free.next;
					list_del(p);
					list_add_before(&pf_used[puser_idx], p);
					// void *cpa = (void*)(msg->req_pid << 22) + (pframe[puser_idx] << 12);
					void *cpa = list_entry(p, Pframe, list)->frame;
					void *ppa = (void*)(ppte->page_frame << 12);
					make_pte(cpte, cpa);
					pframe[puser_idx] ++;
					if(pframe[puser_idx] > 1024)
						assert(0);
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
	msg->dest = msg->src;
	msg->src = MM;
	send(msg->dest, msg);
}

static void reclaim(Msg *msg){
	pid_t req_pid = msg->req_pid;
	uint32_t puser_idx = req_pid - NR_KERNEL_THREAD;
	fetch_pcb(req_pid);
	if(req_pid == 11){
		printk("req_pid 11 reclaim ok\n");
	}
	CR3 *cr3 = fetch_pcb(req_pid)->cr3;
	PDE *pde = (PDE*) pa_to_va(cr3->page_directory_base << 12);
	/* invalidate the pde and pte */
	/* DON'T invalidate pte of kernel */
	int i, j;
	for(i = 0; i < NR_PDE; i++){
		if(pde->present == 1){
			PTE *pte = (PTE*) pa_to_va(pde->page_frame << 12);
			for(j = 0; j < NR_PTE; j++){
				uint32_t addr =  pte->page_frame;
				if(pte->present == 1 && addr >= 10 * 1024){
					make_invalid_pte(pte);
				}
				pte ++;
			}
			make_invalid_pde(pde);
		}
		pde ++;
	}

	/* reclaim to pt_free */
	ListHead *list = pt_used[puser_idx].next;
	while(list != &pt_used[puser_idx]){
		list_del(list);	
		list_add_before(&pt_free, list);
		list = pt_used[puser_idx].next;
	}

	/* reclaim to pf_free */
	list = pf_used[puser_idx].next;
	while(list != &pf_used[puser_idx]){
		list_del(list);	
		list_add_before(&pf_free, list);
		list = pf_used[puser_idx].next;
	}
	msg->dest = msg->src;
	msg->src = MM;
	send(msg->dest, msg);
}

