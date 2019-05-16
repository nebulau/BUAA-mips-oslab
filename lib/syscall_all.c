#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>

extern char *KERNEL_SP;
extern struct Env *curenv;

/* Overview:
 *      This function is used to print a character on screen.
 *
 * Pre-Condition:
 *      `c` is the character you want to print.
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5)
{
        printcharc((char) c);
        return ;
}

/* Overview:
 *      This function enables you to copy content of `srcaddr` to `destaddr`.
 *
 * Pre-Condition:
 *      `destaddr` and `srcaddr` can't be NULL. Also, the `srcaddr` area
 *      shouldn't overlap the `destaddr`, otherwise the behavior of this
 *      function is undefined.
 *
 * Post-Condition:
 *      the content of `destaddr` area(from `destaddr` to `destaddr`+`len`) will
 * be same as that of `srcaddr` area.
 */
void *memcpy(void *destaddr, void const *srcaddr, u_int len)
{
        char *dest = destaddr;
        char const *src = srcaddr;

        while (len-- > 0) {
                *dest++ = *src++;
        }

        return destaddr;
}

/* Overview:
 *      This function provides the environment id of current process.
 *
 * Post-Condition:
 *      return the current environment id
 */
u_int sys_getenvid(void)
{
        return curenv->env_id;
}

/* Overview:
 *      This function enables the current process to give up CPU.
 *
 * Post-Condition:
 *      Deschedule current environment. This function will never return.
 */
void sys_yield(void)
{
        struct Trapframe *src = (struct Trapframe *)(KERNEL_SP - sizeof(struct Trapframe));
    struct Trapframe *dst = (struct Trapframe *)(TIMESTACK - sizeof(struct Trapframe));

        bcopy((void*)src, (void*)dst, sizeof(struct Trapframe));
        sched_yield();
     
}

/* Overview:
 *      This function is used to destroy the current environment.
 *
 * Pre-Condition:
 *      The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 *      Return 0 on success, < 0 when error occurs.
 */
int sys_env_destroy(int sysno, u_int envid)
{
        
        int r;
        struct Env *e;

        if ((r = envid2env(envid, &e, 1)) < 0) {
                return r;
        }

        printf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
        env_destroy(e);
        return 0;
}

/* Overview:
 *      Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 *      xstacktop points one byte past exception stack.
 *
 * Post-Condition:
 *      The envid's pagefault handler will be set to `func` and its
 *      exception stack will be set to `xstacktop`.
 *      Returns 0 on success, < 0 on error.
 */
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
{
        // Your code here.
        struct Env *env;
        int ret;
    if((ret=envid2env(envid,&env,0))!=0) return ret;
    env->env_pgfault_handler = func;
//    xstacktop = TRUP(xstacktop);
    env->env_xstacktop = xstacktop;

        return 0;
        //      panic("sys_set_pgfault_handler not implemented");
}

/* Overview:
 *      Allocate a page of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 *
 *      If a page is already mapped at 'va', that page is unmapped as a
 * side-effect.
 *
 * Pre-Condition:
 * perm -- PTE_V is required,
 *         PTE_COW is not allowed(return -E_INVAL),
 *         other bits are optional.
 *
 * Post-Condition:
 * Return 0 on success, < 0 on error
 *      - va must be < UTOP
 *      - env may modify its own address space or the address space of its children
 *  
 */
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
{
        // Your code here.
        struct Env *env;
        struct Page *ppage;
        int ret;
        ret = 0;
        
        
       if (va >= UTOP || va < 0) {
                printf("va illegal in sys_mem_alloc\n");
                return -E_UNSPECIFIED;
        }

        if ((perm & PTE_COW) || !(perm & PTE_V)) {
                printf("perm illegal in sys_mem_alloc\n");
                return -E_INVAL;
        }

        if (envid2env(envid, &env, 0) < 0) {
                printf("envid illegal in sys_mem_alloc\n");
                return -E_BAD_ENV;
        }

        if (page_alloc(&ppage) < 0) {
                printf("page_alloc failed in sys_mem_alloc\n");
                return -E_NO_MEM;
        }

        if (page_insert(env->env_pgdir, ppage, va, perm) < 0) {     
                printf("page_insert failed in sys_mem_alloc\n");
                return -E_NO_MEM;
        }

        return 0;
}

/* Overview:
 *      Map the page of memory at 'srcva' in srcid's address space
 * at 'dstva' in dstid's address space with permission 'perm'.
 * Perm has the same restrictions as in sys_mem_alloc.
 * (Probably we should add a restriction that you can't go from
 * non-writable to writable?)
 *
 * Post-Condition:
 *      Return 0 on success, < 0 on error.
 *
 * Note:
 *      Cannot access pages above UTOP.
 */
 /* Overview:
 
 */
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
                                u_int perm)
{
    int ret;
    u_int round_srcva, round_dstva;
    struct Env *srcenv;
    struct Env *dstenv;
    struct Page *ppage;
    Pte *ppte;
    //your code here
    ppage = NULL;
    ret = 0;
    round_srcva = ROUNDDOWN(srcva, BY2PG);
    round_dstva = ROUNDDOWN(dstva, BY2PG);
    if (dstva<0||dstva>=UTOP||srcva>=UTOP||srcva<0){printf("illegal addr in sys_mem_map\n"); return -E_INVAL;}
    if ((ret = envid2env(srcid,&srcenv,0))!=0) return ret;
    if ((ret = envid2env(dstid,&dstenv,0))!=0) return ret;
    
    ppage = page_lookup(srcenv->env_pgdir,round_srcva,&ppte);//获取srcva映射的page
    pgdir_walk(srcenv->env_pgdir,srcva,0,&ppte);//获取srcva对应的页表项
    if ( ppte!=NULL && (((*ppte)&PTE_R)==0) && ((perm&PTE_R)!=0) ) return -E_INVAL;//企图从不可写映射到可写,返回错误
    if ((ret = page_insert(dstenv->env_pgdir,ppage,dstva,perm))!=0) return ret;

    return ret; 
            
}

/* Overview:
 *      Unmap the page of memory at 'va' in the address space of 'envid'
 * (if no page is mapped, the function silently succeeds)
 *
 * Post-Condition:
 *      Return 0 on success, < 0 on error.
 *
 * Cannot unmap pages above UTOP.
 */
 /* 
 */
int sys_mem_unmap(int sysno, u_int envid, u_int va)
{
        // Your code here.
        int ret;
        struct Env *env;
    
        if (va >= UTOP || va < 0) {
                printf("va illegal in sys_mem_unmap\n");
                return -E_INVAL;
        }

        if (envid2env(envid, &env, PTE_V) < 0) {
                printf("envid illegal in sys_mem_unmap\n");
                return -E_INVAL;
        }

        page_remove(env -> env_pgdir, va);
        return 0;
}

/* Overview:
 *      Allocate a new environment.
 *
 * Pre-Condition:
 * The new child is left as env_alloc created it, except that
 * status is set to ENV_NOT_RUNNABLE and the register set is copied
 * from the current environment.
 *
 * Post-Condition:
 *      In the child, the register set is tweaked so sys_env_alloc returns 0.
 *      Returns envid of new environment, or < 0 on error.
 */
int sys_env_alloc(void)
{
        // Your code here.
    int r;
    struct Env *e;
    if((r=env_alloc(&e,curenv->env_id))!=0) return r;
    bcopy(KERNEL_SP - sizeof(struct Trapframe), &e->env_tf, sizeof(struct Trapframe));

    e->env_status=ENV_NOT_RUNNABLE;
//    e->env_pgfault_handler=0;
    e->env_tf.pc=e->env_tf.cp0_epc;
    e->env_pri = curenv->env_pri;
    e->env_tf.regs[2]=0;

    return e->env_id;
        //      panic("sys_env_alloc not implemented");
}

/* Overview:
 *      Set envid's env_status to status.
 *
 * Pre-Condition:
 *      status should be one of `ENV_RUNNABLE`, `ENV_NOT_RUNNABLE` and
 * `ENV_FREE`. Otherwise return -E_INVAL.
 *
 * Post-Condition:
 *      Returns 0 on success, < 0 on error.
 *      Return -E_INVAL if status is not a valid status for an environment.
 *      The status of environment will be set to `status` on success.
 */
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
        // Your code here.
        struct Env *env;
        int ret;
    if ((status != ENV_RUNNABLE) && (status != ENV_NOT_RUNNABLE) &&
        (status != ENV_FREE)) {
        return -E_INVAL;
    }

    if ((ret = envid2env(envid, &env, 0)) != 0) {
        return ret;
    }

    env->env_status = status;   
    if(status==ENV_RUNNABLE)LIST_INSERT_HEAD(env_sched_list,env,env_sched_link);
        return 0;
        //      panic("sys_env_set_status not implemented");
}

/* Overview:
 *      Set envid's trap frame to tf.
 *
 * Pre-Condition:
 *      `tf` should be valid.
 *
 * Post-Condition:
 *      Returns 0 on success, < 0 on error.
 *      Return -E_INVAL if the environment cannot be manipulated.
 *
 * Note: This hasn't be used now?
 */
int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf)
{

        return 0;
}

/* Overview:
 *      Kernel panic with message `msg`.
 *
 * Pre-Condition:
 *      msg can't be NULL
 *
 * Post-Condition:
 *      This function will make the whole system stop.
 */
void sys_panic(int sysno, char *msg)
{
        // no page_fault_mode -- we are trying to panic!
        panic("%s", TRUP(msg));
}

/*
 u_int env_ipc_value;    
    u_int env_ipc_from;     
    u_int env_ipc_recving;  
    u_int env_ipc_dstva;    
    u_int env_ipc_perm;     
*/
/* Overview:
 *      This function enables caller to receive message from
 * other process. To be more specific, it will flag
 * the current process so that other process could send
 * message to it.
 *
 * Pre-Condition:
 *      `dstva` is valid (Note: NULL is also a valid value for `dstva`).
 *
 * Post-Condition:
 *      This syscall will set the current process's status to
 * ENV_NOT_RUNNABLE, giving up cpu.
 */
void sys_ipc_recv(int sysno, u_int dstva)
{
    
    if (dstva >= UTOP) 
                return;
        
        curenv -> env_ipc_recving = 1;
        curenv -> env_ipc_dstva = dstva;
        curenv -> env_status = ENV_NOT_RUNNABLE;
        sys_yield();
}

/* Overview:
 *      Try to send 'value' to the target env 'envid'.
 *      The send fails with a return value of -E_IPC_NOT_RECV if the
 * target has not requested IPC with sys_ipc_recv
 *      Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends
 *    env_ipc_from is set to the sending envid
 *    env_ipc_value is set to the 'value' parameter
 *      The target environment is marked runnable again.
 *
 * Post-Condition:
 *      Return 0 on success, < 0 on error.
 *
 * Hint: the only function you need to call is envid2env.
 */
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
                     u_int perm)
{
    //int r;
    struct Env *e;
    struct Page *p;
    Pte *ppte;
    if(srcva<0)return -E_IPC_NOT_RECV;
    if(srcva>=UTOP)return -E_INVAL;
    if(envid2env(envid,&e,0)<0)return -E_INVAL;
    if(e->env_ipc_recving==0)return -E_IPC_NOT_RECV;
    if((p=page_lookup(curenv->env_pgdir,srcva,0))<=0){
    ;// printf("non-exitent srcva:%x\n",srcva);
    }else if(page_insert(e->env_pgdir,p,e->env_ipc_dstva,perm)<0){
    ;// printf("dst pot failed.\n");
        return -E_INVAL;
    }
    e->env_ipc_perm = perm|PTE_V|PTE_R;
    e->env_ipc_recving = 0;
    e->env_status = ENV_RUNNABLE;
    e->env_ipc_value = value;
    e->env_ipc_from = curenv->env_id;
    return 0;
}
/* Overview:
 *      This function is used to write data to device, which is
 *      represented by its mapped physical address.
 * 
 * Pre-Condition:
 *      'va' is the startting address of source data, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 *      
 * Post-Condition:
 *      copy data from 'va' to 'dev' with length 'len'
 *      Return 0 on success, < 0 on error
 *      
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 *       Data can cross pages.
 */
int sys_write_dev(int sysno, u_int va, u_int dev, u_int len)
{
    if (va >= UTOP || va < 0)   
            return -E_INVAL;
    if (!((dev >= 0x10000000) && (dev < 0x10000020)) && !((dev >= 0x13000000) && (dev < 0x13004200)) && !((dev >= 0x15000000) && (dev < 0x15000200))) 
        return -E_INVAL;
    if ((dev >= 0x10000000) && (dev < 0x10000020)) 
        if ((dev + len) > 0x10000020) 
            return -E_INVAL;
    if ((dev >= 0x13000000) && (dev < 0x13004200)) 
        if ((dev + len) > 0x13004200) 
            return -E_INVAL;
    if ((dev >= 0x15000000) && (dev < 0x15000200)) 
        if ((dev + len) > 0x15000200) 
            return -E_INVAL;

        // Your code here
    struct Page *ppage;
    if(curenv==NULL)panic("null curenv!");
        u_long i;
    u_long offset = va - ROUNDDOWN(va,BY2PG);

    for (i = 0; ((i>offset)?i-offset:0) < len; i += BY2PG) {
        ppage=page_lookup(curenv->env_pgdir,va+i,0);
        u_long kva=page2kva(ppage)+offset;

        if (i==0) {
            bcopy((void*)kva, (void *)(dev+0xA0000000), (len>BY2PG-offset)?BY2PG-offset:len);
        } else {
            bcopy((void*)(kva-offset), (void *)(dev+0xA0000000+i-offset), (len-i+offset>BY2PG)?BY2PG:len-i+offset);
        }
     }
/*
    ppage=page_lookup(curenv->env_pgdir,va,&ppte);
    u_long kva=page2kva(ppage);

    kva=kva+va-ROUNDDOWN(va,BY2PG);
    bcopy((void*)kva,(void*)(dev+0xA0000000),len);
*/
    return 0;
}

/* Overview:
 *      This function is used to read data from device, which is
 *      represented by its mapped physical address.
 * 
 * Pre-Condition:
 *      'va' is the startting address of data buffer, 'len' is the
 *      length of data (in bytes), 'dev' is the physical address of
 *      the device
 * 
 * Post-Condition:
 *      copy data from 'dev' to 'va' with length 'len'
 *      Return 0 on success, < 0 on error
 *      
 * Hint: Use ummapped segment in kernel address space to perform MMIO.
 *       Data can cross pages.
 */
int sys_read_dev(int sysno, u_int va, u_int dev, u_int len)
{
    if (va >= UTOP || va < 0)   return -E_INVAL;
    if (!((dev >= 0x10000000) && (dev < 0x10000020)) && !((dev >= 0x13000000) && (dev < 0x13004200)) && !((dev >= 0x15000000) && (dev < 0x15000200)))
        return -E_INVAL;
    if ((dev >= 0x10000000) && (dev < 0x10000020)) 
        if ((dev + len) > 0x10000020) 
            return -E_INVAL;
    if ((dev >= 0x13000000) && (dev < 0x13004200)) 
        if ((dev + len) > 0x13004200) 
            return -E_INVAL;
    if ((dev >= 0x15000000) && (dev < 0x15000200)) 
        if ((dev + len) > 0x15000200) 
            return -E_INVAL;

        struct Page *ppage;
        if(curenv==NULL)panic("null curenv!");
    u_long i;
        u_long offset = va - ROUNDDOWN(va,BY2PG);

    for (i = 0; ((i>offset)?i-offset:0) < len; i += BY2PG) {
        ppage=page_lookup(curenv->env_pgdir,va+i,0);
        u_long kva=page2kva(ppage)+offset;

        if (i==0) {
            bcopy((void*)(dev+0xA0000000), (void *)kva, (len>BY2PG-offset)?BY2PG-offset:len);
        } else {
            bcopy((void *)(dev+0xA0000000+i-offset), (void*)(kva-offset), (len-i+offset>BY2PG)?BY2PG:len-i+offset);
        }
     }
/*
        ppage=page_lookup(curenv->env_pgdir,va,&ppte);
        u_long kva=page2kva(ppage);
    kva=kva+va-ROUNDDOWN(va,BY2PG);
        bcopy((void*)(dev+0xA0000000),(void*)kva,len);
*/
        return 0;
}
int sys_read_sector(int sysno,int diskno,int offset){
    void* dst=0xB3000010;
    *(int *)dst=diskno;
    dst=0xB3000000;
    *(int *)dst=offset;
    dst=0xB3000020;
    *(char *)dst=0;
    int v0=*(int *)0xB3000030;
    return v0;
}
int sys_write_sector(int sysno,int diskno,int offset){
        void* dst=0xB3000010;
        *(int *)dst=diskno;
        dst=0xB3000000;
        *(int *)dst=offset;
        dst=0xB3000020;
        *(char *)dst=1;
        int v0=*(int *)0xB3000030;
        return v0;
}
