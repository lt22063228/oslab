#include "kernel.h"
#include "hal.h"
#include "memory.h"
#include "x86/memory.h"

#define NR_MEM_SEG (PHY_MEM / (4096*1024) )
#define NR_MEM_PAGE (PHY_MEM / 4096)
#define SEG_SIZE ( 4096 * 1024 )
#define NR_SEG_PAGE 1024
#define NR_USER_PROC 1
#define NR_KERNEL_THREAD 10

pid_t MM;
uint8_t alloc_map[NR_MEM_PAGE] = {0};
static CR3 ucr3[NR_USER_PROC];
static PDE updir[NR_USER_PROC][NR_PDE] align_to_page;
static PTE uptable[NR_USER_PROC][NR_PDE][NR_PTE] align_to_page;
//static uint32_t pframe[NR_USER_PROC]
static void mmd(void);

void init_mm(void) {
	uint32_t puser_idx, pdir_idx, ptable_idx;
	for( puser_idx = 0; puser_idx < NR_USER_PROC; puser_idx ++){
		ucr3[puser_idx].val = 0;
		ucr3[puser_idx].page_directory_base = (uint32_t)( va_to_pa( &updir[puser_idx] ) ) >> 12 ;
		for( pdir_idx = 0; pdir_idx < NR_PDE; pdir_idx ++ ){
			make_invalid_pde( (PDE*)va_to_pa(&updir[puser_idx][pdir_idx]) );
			for( ptable_idx = 0; ptable_idx < NR_PTE; ptable_idx ++){
				make_invalid_pte( (PTE*)va_to_pa(&uptable[puser_idx][pdir_idx][ptable_idx]) );
			}
		}
	}
	
	PCB *p = create_kthread( mmd );
	MM = p->pid;
	wakeup(p);
}
static void
mmd(void){
	Msg m;
	while(1){
		receive( ANY, &m );
		pid_t req_pid;
		uint32_t va;
		uint32_t puser_idx, pdir_idx, ptable_idx, pframe_idx;
		size_t memsz;
		int num_page;
		int i;
		switch( m.type ){
			case MAP_KERNEL:
				req_pid = m.req_pid;
				puser_idx = req_pid - NR_KERNEL_THREAD;
				PTE *ptable = (PTE *)va_to_pa(get_kptable());
				for (pdir_idx = 0; pdir_idx < PHY_MEM / PD_SIZE; pdir_idx ++) {
					make_pde(&updir[puser_idx][pdir_idx], ptable + (pdir_idx * 1024) );
					make_pde(&updir[puser_idx][pdir_idx + KOFFSET / PD_SIZE], ptable + (pdir_idx * 1024));
				}
				m.dest = m.src;
				m.src = MM;
				send( m.dest, &m );
				break;
			case NEW_PAGE:
				req_pid = m.req_pid;
				va = m.offset;
				memsz = m.len;
				num_page = memsz % PAGE_SIZE == 0 ? memsz / PAGE_SIZE : memsz / PAGE_SIZE + 1;
				puser_idx = req_pid - NR_KERNEL_THREAD;
				pframe_idx = req_pid * SEG_SIZE / PAGE_SIZE;
				for( ; alloc_map[pframe_idx] == 1; pframe_idx ++ );
				for( i=0, va=m.offset; i < num_page; i++, va += PAGE_SIZE){
					pdir_idx = va >> 22;
					PDE *ppde = &updir[puser_idx][pdir_idx];
					if( ppde-> present == 0 ){
						make_pde( ppde, va_to_pa(&uptable[puser_idx][pdir_idx]) );
					}
					ptable_idx = ( va >> 12 ) & 0x3ff;
					PTE *ppte = &uptable[puser_idx][pdir_idx][ptable_idx];
					if( ppte -> present == 0 ){
						make_pte( ppte, (void*)( (pframe_idx+i) << 12) );
					}
					alloc_map[ pframe_idx + i ] = 1;
				}	
				m.ret = (int)(pframe_idx << 12);
				m.dest = m.src;
				m.src = MM;
				send( m.dest, &m );
				break;
			case NEW_PCB:
				puser_idx = m.req_pid - NR_KERNEL_THREAD;
				m.dest = m.src;
				m.ret = (int)&ucr3[puser_idx];
				m.src = MM;
				send( m.dest, &m );
				break;
			default: assert(0);
		}
	}
}
