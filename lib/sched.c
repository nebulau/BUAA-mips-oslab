#include <env.h>
#include <pmap.h>
#include <printf.h>

/* Overview:
 *  Implement simple round-robin scheduling.
 *  Search through 'envs' for a runnable environment ,
 *  in circular fashion statrting after the previously running env,
 *  and switch to the first such environment found.
 *
 * Hints:
 *  The variable which is for counting should be defined as 'static'.
 */
void sched_yield(void) { 
    static struct Env *cur = NULL; 
    static u_int curtime = 0; 
    static int x = 0; 
    while (curtime <= 0 || cur && cur -> env_status != ENV_RUNNABLE) { 
        if (cur != NULL) { 
            LIST_REMOVE(cur, env_sched_link); 
            LIST_INSERT_HEAD(&env_sched_list[1-x], cur, env_sched_link); 
        } 
        while ((LIST_FIRST(&env_sched_list[x])) == NULL) { 
            x = 1 - x; 
        } 
        cur = LIST_FIRST(&env_sched_list[x]); 
        curtime = cur -> env_pri; 
    } 
    curtime--; 
    env_run(cur); 
}
