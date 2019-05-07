#include "mmu.h"
#include "pmap.h"
#include "printf.h"
#include "env.h"
#include "error.h"



/* These variables are set by mips_detect_memory() */
u_long maxpa;            /* Maximum physical address */
u_long npage;            /* Amount of memory(in pages) */
u_long basemem;          /* Amount of base memory(in bytes) */
u_long extmem;           /* Amount of extended memory(in bytes) */

Pde *boot_pgdir;

struct Page *pages;
static u_long freemem;

static struct Page_list page_free_list;	/* Free list of physical pages */


/* Overview:
 	Initialize basemem and npage.
 	Set basemem to be 64MB, and calculate corresponding npage value.*/
void mips_detect_memory()
{
    /* Step 1: Initialize basemem.
     * (When use real computer, CMOS tells us how many kilobytes there are). */
	maxpa = 0x04000000;
	basemem = 64*1024*1024;
    // Step 2: Calculate corresponding npage value.
	npage = basemem/(1024*4);
    printf("Physical memory: %dK available, ", (int)(maxpa / 1024));
    printf("base = %dK, extended = %dK\n", (int)(basemem / 1024),
           (int)(extmem / 1024));
}

/* Overview:
 	Allocate `n` bytes physical memory with alignment `align`, if `clear` is set, clear the
 	allocated memory.
 	This allocator is used only while setting up virtual memory system.

   Post-Condition:
	If we're out of memory, should panic, else return this address of memory we have allocated.*/
static void *alloc(u_int n, u_int align, int clear)
{
    extern char end[];
    u_long alloced_mem;

    /* Initialize `freemem` if this is the first time. The first virtual address that the
     * linker did *not* assign to any kernel code or global variables. */
    if (freemem == 0) {
        freemem = (u_long)end;
    }

    /* Step 1: Round up `freemem` up to be aligned properly */
    freemem = ROUND(freemem, align);

    /* Step 2: Save current value of `freemem` as allocated chunk. */
    alloced_mem = freemem;

    /* Step 3: Increase `freemem` to record allocation. */
    freemem = freemem + n;

    /* Step 4: Clear allocated chunk if parameter `clear` is set. */
    if (clear) {
        bzero((void *)alloced_mem, n);
    }

    // We're out of memory, PANIC !!
    if (PADDR(freemem) >= maxpa) {
        panic("out of memorty\n");
        return (void *)-E_NO_MEM;
    }

    /* Step 5: return allocated chunk. */
    return (void *)alloced_mem;
}

/* Overview:
 	Get the page table entry for virtual address `va` in the given
 	page directory `pgdir`.
	If the page table is not exist and the parameter `create` is set to 1,
	then create it.*/
static Pte *boot_pgdir_walk(Pde *pgdir, u_long va, int create)
{

    Pde *pgdir_entryp;//页目录入口地址(指针)
    Pte *pgtable, *pgtable_entryp;

    /* Step 1: Get the corresponding page directory entry and page table. */
    /* Hint: Use KADDR and PTE_ADDR to get the page table from page directory
     * entry value. */
	pgdir_entryp = (Pde *)&pgdir[PDX(va)];//pgdir在mips_vm_init()中分配，对应地址是虚拟地址
	pgtable = (Pte *)KADDR(PTE_ADDR(*pgdir_entryp));//to virtual address

    /* Step 2: If the corresponding page table is not exist and parameter `create`
     * is set, create one. And set the correct permission bits for this new page
     * table. */
	if((*pgdir_entryp & PTE_V)==0x0){
		if(create){
			pgtable = alloc(BY2PG,BY2PG,1);//返回的是虚拟地址，需要使用PADDR函数
			*pgdir_entryp =(Pde*)(PADDR(pgtable)|PTE_V|PTE_R); //由于页大小为4KB，所以物理地址的低12为本全为0，正好可以用于设置符号位.
		}else{
			return 0;
		}
	}

    /* Step 3: Get the page table entry for `va`, and return it. */
	pgtable_entryp =(Pte *) &pgtable[PTX(va)];
	return pgtable_entryp;
}

/*Overview:
 	Map [va, va+size) of virtual address space to physical [pa, pa+size) in the page
	table rooted at pgdir.
	Use permission bits `perm|PTE_V` for the entries.
 	Use permission bits `perm` for the entries.

  Pre-Condition:
	Size is a multiple of BY2PG.*/
void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)
{
	// perm actual is PTE_R 修改位
    int i;
    Pte *pgtable_entry;

    /* Step 1: Check if `size` is a multiple of BY2PG. */
	if(size%BY2PG!=0)
		printf("the size is not multiple of BY2PG\n");
    /* Step 2: Map virtual address space to physical address. */
    /* Hint: Use `boot_pgdir_walk` to get the page table entry of virtual address `va`. */
	for(i=0;i<size/BY2PG;i++){
		pgtable_entry = boot_pgdir_walk(pgdir,va+i*BY2PG,1);
		*pgtable_entry = (pa + i*BY2PG)|perm|PTE_V|PTE_R;
	}
	pgdir[PDX(va)] |= perm|PTE_V|PTE_R;
}

/* Oierview:
    Set up two-level page table.

   Hint:  
    You can get more details about `UPAGES` and `UENVS` in include/mmu.h. */
void mips_vm_init()
{
    extern char end[];
    extern int mCONTEXT;
    extern struct Env *envs;

    Pde *pgdir;
   	u_int n;

    /* Step 1: Allocate a page for page directory(first level page table). */
    pgdir = alloc(BY2PG, BY2PG, 1);
    printf("to memory %x for struct page directory.\n", freemem);
    mCONTEXT = (int)pgdir;

    boot_pgdir = pgdir;

    /* Step 2: Allocate proper size of physical memory for global array `pages`,
     * for physical memory management. Then, map virtual address `UPAGES` to
     * physical address `pages` allocated before. For consideration of alignment,
     * you should round up the memory size before map. */
    pages = (struct Page *)alloc(npage * sizeof(struct Page), BY2PG, 1);
    printf("to memory %x for struct Pages.\n", freemem);
    n = ROUND(npage * sizeof(struct Page), BY2PG);
    boot_map_segment(pgdir, UPAGES, n, PADDR(pages), PTE_R);

    /* Step 3, Allocate proper size of physical memory for global array `envs`,
     * for process management. Then map the physical address to `UENVS`. */
    envs = (struct Env *)alloc(NENV * sizeof(struct Env), BY2PG, 1);
    n = ROUND(NENV * sizeof(struct Env), BY2PG);
    boot_map_segment(pgdir, UENVS, n, PADDR(envs), PTE_R);
	printf("mips_vm_init:boot_pgdir is %x\n",boot_pgdir);
    printf("pmap.c:\t mips vm init success\n");
}

/*Overview:
 	Initialize page structure and memory free list.
 	The `pages` array has one `struct Page` entry per physical page. Pages
	are reference counted, and free pages are kept on a linked list.
  Hint:
	Use `LIST_INSERT_HEAD` to insert something to list.*/
void
page_init(void)
{
    /* Step 1: Initialize page_free_list. */
    /* Hint: Use macro `LIST_INIT` defined in include/queue.h. */
	LIST_INIT(&page_free_list);//because page_free_list is just a struct,but not point.

    /* Step 2: Align `freemem` up to multiple of BY2PG. */
	freemem = ROUND(freemem,BY2PG);//ROUND() in mmu.h	

    /* Step 3: Mark all memory block `freemem` as used(set `pp_ref`
     * filed to 1) */
	/*I think it is for kernel*/
	int loop;
	int loopLength = PADDR(freemem)/BY2PG;
	for(loop=0;loop<loopLength;loop++){
		pages[loop].pp_ref = 1; 
	} //because these blocks are for kernel(in low physical address)
	/* Step 4: Mark the other memory as free. */
	/*I think it is for others not in kernel*/
	for(loop=loopLength;loop<npage;loop++){
		pages[loop].pp_ref = 0;
		LIST_INSERT_HEAD(&page_free_list,&pages[loop],pp_link);//insert into free list.
	}
	//may be all have done.
}

/*Overview:
	Allocates a physical page from free memory, and clear this page.

  Post-Condition:
	If failed to allocate a new page(out of memory(there's no free page)),
 	return -E_NO_MEM.
	Else, set the address of allocated page to *pp, and returned 0.

  Note:
 	Does NOT increment the reference count of the page - the caller must do
 	these if necessary (either explicitly or via page_insert).

  Hint:
	Use LIST_FIRST and LIST_REMOVE defined in include/queue.h .*/
int
page_alloc(struct Page **pp)
{
	//this function in lab2 position at page_check()
    struct Page *ppage_temp;
    /* Step 1: Get a page from free memory. If fails, return the error code.*/
	if((ppage_temp=LIST_FIRST(&page_free_list))!=NULL){
		*pp = ppage_temp;//get and turn for the *pp
		LIST_REMOVE(ppage_temp,pp_link);//remove allocted page from free_list.
		/* Step 2: Initialize this page.* Hint: use `bzero`. */
		u_long pa_pp = page2pa(ppage_temp);//get the physical address of new alloc page
		u_long va_pp = KADDR(pa_pp);//get the virtual address of new alloc page because we need use bzero()
		bzero((void *)va_pp,BY2PG);//init ok!
		return 0;
	}	
	return -E_NO_MEM;// -4
}

/*Overview:
	Release a page, mark it as free if it's `pp_ref` reaches 0.
  Hint:
	When to free a page, just insert it to the page_free_list.*/
void
page_free(struct Page *pp)
{
    /* Step 1: If there's still virtual address refers to this page, do nothing. */
	if(pp->pp_ref > 0 ) 
		return;
    /* Step 2: If the `pp_ref` reaches to 0, mark this page as free and return. */
	else if(pp->pp_ref == 0)
	{
		LIST_INSERT_HEAD(&page_free_list,pp,pp_link);
		return;
	}
    /* If the value of `pp_ref` less than 0, some error must occurred before,
     * so PANIC !!! */
    panic("cgh:pp->pp_ref is less than zero\n");
}

/*Overview:
 	Given `pgdir`, a pointer to a page directory, pgdir_walk returns a pointer
 	to the page table entry (with permission PTE_R|PTE_V) for virtual address 'va'.

  Pre-Condition:
	The `pgdir` should be two-level page table structure.

  Post-Condition:
 	If we're out of memory, return -E_NO_MEM.
	Else, we get the page table entry successfully, store the value of page table
	entry to *ppte, and return 0, indicating success.

  Hint:
	We use a two-level pointer to store page table entry and return a state code to indicate
	whether this function execute successfully or not.
    This function have something in common with function `boot_pgdir_walk`.*/
int
pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)
{
    Pde *pgdir_entryp;
    Pte *pgtable;
    struct Page *ppage;//a temp point,help for ppte

    /* Step 1: Get the corresponding page directory entry and page table. */
	pgdir_entryp = (Pde *)&(pgdir[PDX(va)]);
	pgtable = (Pte *)KADDR(PTE_ADDR(*pgdir_entryp));
	/*by the va,get the pa; reagain,by the pa,get the va of the two-level page table array*/
	//use PTE_ADDR is because the low-12bit of pa is not all 0,need change.

    /* Step 2: If the corresponding page table is not exist(valid) and parameter `create`
     * is set, create one. And set the correct permission bits for this new page
     * table.
     * When creating new page table, maybe out of memory. */
	if((*pgdir_entryp & PTE_V)==0x0){
		if(create){
			int status = page_alloc(&ppage);
			if(status == 0 ){
				u_long ppage_pa = page2pa(ppage);
				u_long ppage_va = KADDR(ppage_pa);
				pgtable = ppage_va;
				*pgdir_entryp = PADDR(pgtable)|PTE_V;
				ppage->pp_ref+=1;  //计数器加一
			}else{
				*ppte = 0x0;
				return -E_NO_MEM;
			}
		}else{
			*ppte = 0x0;
			return 0;
		}
	}

    /* Step 3: Set the page table entry to `*ppte` as return value. */
	Pte * pgtable_entryp = (Pte *)&pgtable[PTX(va)];
	*ppte = pgtable_entryp;
    return 0;
}

/*Overview:
 	Map the physical page 'pp' at virtual address 'va'.
 	The permissions (the low 12 bits) of the page table entry should be set to 'perm|PTE_V'.

  Post-Condition:
    Return 0 on success
    Return -E_NO_MEM, if page table couldn't be allocated

  Hint:
	If there is already a page mapped at `va`, call page_remove() to release this mapping.
	The `pp_ref` should be incremented if the insertion succeeds.*/
int
page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm)
{
    u_int PERM;
    Pte *pgtable_entry;
    PERM = perm | PTE_V;
    /* Step 1: Get corresponding page table entry. */
    pgdir_walk(pgdir, va, 0, &pgtable_entry);

    if (pgtable_entry != 0 && (*pgtable_entry & PTE_V) != 0) {
        if (pa2page(*pgtable_entry) != pp) {
            page_remove(pgdir, va);
        } else	{
            tlb_invalidate(pgdir, va);
            *pgtable_entry = (page2pa(pp) | PERM);
            return 0;
        }
    }

    /* Step 2: Update TLB. */
    tlb_invalidate(pgdir, va);
    /* Step 3: Do check, re-get page table entry to validate the insertion. */
    if (pgdir_walk(pgdir, va, 1, &pgtable_entry) != 0) {
        return -E_NO_MEM;    // panic ("page insert failed .\n");
    }

    *pgtable_entry = (page2pa(pp) | PERM);
    pp->pp_ref++;
    return 0;
}

/*Overview:
	Look up the Page that virtual address `va` map to.

  Post-Condition:
	Return a pointer to corresponding Page, and store it's page table entry to *ppte.
	If `va` doesn't mapped to any Page, return NULL.*/
struct Page *
page_lookup(Pde *pgdir, u_long va, Pte **ppte)
{
    struct Page *ppage;
    Pte *pte;
    /* Step 1: Get the page table entry. */
    pgdir_walk(pgdir, va, 0, &pte);

    /* Hint: Check if the page table entry doesn't exist or is not valid. */
    if (pte == 0) {
        return 0;
    }
    if ((*pte & PTE_V) == 0) {
        return 0;    //the page is not in memory.
    }

    /* Step 2: Get the corresponding Page struct. */

    /* Hint: Use function `pa2page`, defined in include/pmap.h . */
    ppage = pa2page(*pte);
    if (ppte) {
        *ppte = pte;
    }

    return ppage;
}

// Overview:
// 	Decrease the `pp_ref` value of Page `*pp`, if `pp_ref` reaches to 0, free this page.
void page_decref(struct Page *pp) {
    if(--pp->pp_ref == 0) {
        page_free(pp);
    }
}

// Overview:
// 	Unmaps the physical page at virtual address `va`.
void
page_remove(Pde *pgdir, u_long va)
{
    Pte *pagetable_entry;
    struct Page *ppage;

    /* Step 1: Get the page table entry, and check if the page table entry is valid. */
    ppage = page_lookup(pgdir, va, &pagetable_entry);

    if (ppage == 0) {
        return;
    }

    /* Step 2: Decrease `pp_ref` and decide if it's necessary to free this page. */

    /* Hint: When there's no virtual address mapped to this page, release it. */
    ppage->pp_ref--;
    if (ppage->pp_ref == 0) {
        page_free(ppage);
    }

    /* Step 3: Update TLB. */
    *pagetable_entry = 0;
    tlb_invalidate(pgdir, va);
    return;
}

// Overview:
// 	Update TLB.
void
tlb_invalidate(Pde *pgdir, u_long va)
{
    if (curenv) {
		//printf("curenv->id in tlb_invalidate:%d\n",curenv->env_id);
        tlb_out(PTE_ADDR(va) | GET_ENV_ASID(curenv->env_id));
    } else {
        tlb_out(PTE_ADDR(va));
    }
}


void
page_check(void)
{
    struct Page *pp, *pp0, *pp1, *pp2;
    struct Page_list fl;

    // should be able to allocate three pages
    pp0 = pp1 = pp2 = 0;
    assert(page_alloc(&pp0) == 0);
    assert(page_alloc(&pp1) == 0);
    assert(page_alloc(&pp2) == 0);

    assert(pp0);
    assert(pp1 && pp1 != pp0);
    assert(pp2 && pp2 != pp1 && pp2 != pp0);

    // temporarily steal the rest of the free pages
    fl = page_free_list;
    // now this page_free list must be empty!!!!
    LIST_INIT(&page_free_list);

    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    // there is no free memory, so we can't allocate a page table
    assert(page_insert(boot_pgdir, pp1, 0x0, 0) < 0);

    // free pp0 and try again: pp0 should be used for page table
    page_free(pp0);
    assert(page_insert(boot_pgdir, pp1, 0x0, 0) == 0);
    assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));

    printf("va2pa(boot_pgdir, 0x0) is %x\n",va2pa(boot_pgdir, 0x0));
    printf("page2pa(pp1) is %x\n",page2pa(pp1));

    assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
    assert(pp1->pp_ref == 1);

    // should be able to map pp2 at BY2PG because pp0 is already allocated for page table
    assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
    assert(pp2->pp_ref == 1);

    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    printf("start page_insert\n");
    // should be able to map pp2 at BY2PG because it's already there
    assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));
    assert(pp2->pp_ref == 1);

    // pp2 should NOT be on the free list
    // could happen in ref counts are handled sloppily in page_insert
    assert(page_alloc(&pp) == -E_NO_MEM);

    // should not be able to map at PDMAP because need free page for page table
    assert(page_insert(boot_pgdir, pp0, PDMAP, 0) < 0);

    // insert pp1 at BY2PG (replacing pp2)
    assert(page_insert(boot_pgdir, pp1, BY2PG, 0) == 0);

    // should have pp1 at both 0 and BY2PG, pp2 nowhere, ...
    assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
    // ... and ref counts should reflect this
    assert(pp1->pp_ref == 2);
    printf("pp2->pp_ref %d\n",pp2->pp_ref);
    assert(pp2->pp_ref == 0);
    printf("end page_insert\n");

    // pp2 should be returned by page_alloc
    assert(page_alloc(&pp) == 0 && pp == pp2);

    // unmapping pp1 at 0 should keep pp1 at BY2PG
    page_remove(boot_pgdir, 0x0);
    assert(va2pa(boot_pgdir, 0x0) == ~0);
    assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));
    assert(pp1->pp_ref == 1);
    assert(pp2->pp_ref == 0);

    // unmapping pp1 at BY2PG should free it
    page_remove(boot_pgdir, BY2PG);
    assert(va2pa(boot_pgdir, 0x0) == ~0);
    assert(va2pa(boot_pgdir, BY2PG) == ~0);
    assert(pp1->pp_ref == 0);
    assert(pp2->pp_ref == 0);

    // so it should be returned by page_alloc
    assert(page_alloc(&pp) == 0 && pp == pp1);

    // should be no free memory
    assert(page_alloc(&pp) == -E_NO_MEM);

    // forcibly take pp0 back
    assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));
    boot_pgdir[0] = 0;
    assert(pp0->pp_ref == 1);
    pp0->pp_ref = 0;

    // give free list back
    page_free_list = fl;

    // free the pages we took
    page_free(pp0);
    page_free(pp1);
    page_free(pp2);
	/*u_long *va = 0x00012450;
	u_long *pa;
	page_insert(boot_pgdir,pp,va,PTE_R);
	pa = va2pa(boot_pgdir,va);
	printf("va: %x -> pa: %x\n",va,pa);
	*va = 0x88888;
	printf("va value: %x\n",va);
	printf("pa value: %x\n",(va2pa(boot_pgdir,va)));*/
    printf("page_check() succeeded!\n");
}

void pageout(int va, int context)
{
    u_long r;
    struct Page *p = NULL;
    if (context < 0x80000000) {
        panic("tlb refill and alloc error!");
    }

    if ((va > 0x7f400000) && (va < 0x7f800000)) {
        panic(">>>>>>>>>>>>>>>>>>>>>>it's env's zone");
    }
    if (va < 0x10000) {
		printf("curenv->id:%d,va:%d,context:%x,status:%d\n",curenv->env_id,va,context,curenv->env_status);
        panic("^^^^^^TOO LOW^^^^^^^^^");
    }

    if ((r = page_alloc(&p)) < 0) {
        panic ("page alloc error!");
    }

    p->pp_ref++;

    page_insert((Pde *)context, p, VA2PFN(va), PTE_R);
    printf("pageout:\t@@@___0x%x___@@@  ins a page \n", va);
}

