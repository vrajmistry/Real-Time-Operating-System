/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

extern void kcd_task(void);


/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks
int				filled[MAX_TASKS];

typedef struct __task_node{
    TCB* current;
    struct __task_node* next;
}task_node;

task_node curr3;
task_node curr2;

TCB* prioq[256];

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
	int pos;
	for (pos = 0; pos < 256; pos++) {
		if (prioq[pos] != NULL) {
			if (prioq[pos]->state == READY) {
				break;
			}
		}
	}
	if (pos == 256){
		pos = 255; // null_tsk
	}
	TCB *new_task = prioq[pos];

    return new_task;


}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int add_to_prioq(TCB *new, int prio) {
	if (prioq[prio] == NULL) {
		prioq[prio] = new;
		return 0;
	}
	TCB *head = prioq[prio];
	while(head->next != NULL) {
		head = head->next;
	}
	head->next = new;
	return 0;
}

int remove_from_prioq(task_t old, int prio) {
	if (prioq[prio] == NULL) {
		return RTX_ERR;
	}
	TCB *head = prioq[prio];
	if (head->tid == old) {
		if (head->next == NULL){
		}
		prioq[prio] = head->next;
		if (prioq[prio] == NULL){
		}
		return RTX_OK;
	}
	while(head->next != NULL) {
		if (head->next->tid == old) {
			head->next = head->next->next;
			break;
		}
	}
	return RTX_OK;
}


int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
	for (int i = 0; i < 256; i++) {
		prioq[i] = NULL;
	}
	for (int i = 0; i < 160; i++) {
			filled[i] = 0;
		}
    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the Null task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    p_tcb->func    = task_null;
    p_tcb->k_stack_hi = g_null_task_info.k_stack_hi;
    p_tcb->k_stack_size = g_null_task_info.k_stack_size;

    p_tcb->u_stack_hi = g_null_task_info.u_stack_hi;
    p_tcb->u_stack_size = g_null_task_info.u_stack_size;


    g_num_active_tasks++;
    gp_current_task = p_tcb;


    p_tcb->next = NULL;
    prioq[PRIO_NULL] = p_tcb;
    filled[0] = 1;

    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
    	int k = i+1;

    	if (p_taskinfo->ptask == &kcd_task){ //kcd task creation
			TCB *p_tcb2 = &g_tcbs[TID_KCD];
			p_tcb2->u_stack_size = p_taskinfo->u_stack_size;

			if (k_tsk_create_new(p_taskinfo, p_tcb2, TID_KCD) == RTX_OK) {
				g_num_active_tasks++;
			}
			p_tcb2->tid = TID_KCD;
			p_tcb2->prio = p_taskinfo->prio;
			p_tcb2->state = READY;
			p_tcb2->func = p_taskinfo->ptask;
			p_tcb2->priv = p_taskinfo->priv;
			p_tcb2->next = NULL;
			p_tcb2->check = 0;

			add_to_prioq(p_tcb2, p_taskinfo->prio);
			filled[TID_KCD] = 1;
			p_taskinfo++;
			}
		else{
			if (p_taskinfo->prio == PRIO_NULL){ //  rejected attempt to create NULL TASK
				p_taskinfo++;
				i--;
				num_tasks--;
				continue;
			}

			TCB *p_tcb2 = &g_tcbs[k];
			p_tcb2->u_stack_size = p_taskinfo->u_stack_size;

			if (k_tsk_create_new(p_taskinfo, p_tcb2, k) == RTX_OK) {
				g_num_active_tasks++;
			}
			p_tcb2->tid = k;
			p_tcb2->prio = p_taskinfo->prio;
			p_tcb2->state = READY;
			p_tcb2->func = p_taskinfo->ptask;
			p_tcb2->priv = p_taskinfo->priv;
			p_tcb2->next = NULL;
            p_tcb2->check = 0;
			//p_tcb2->k_stack_hi = (U32)k_alloc_k_stack(p_tcb2->tid);
			add_to_prioq(p_tcb2, p_taskinfo->prio);
			filled[k] = 1;
			p_taskinfo++;
		}
    }

    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    p_tcb ->tid = tid;
    p_tcb->state = READY;

    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) = (U32) k_alloc_p_stack(tid);

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);


    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;

    return RTX_OK;
}
/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    if ( p_tcb_old == NULL  ) {
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        if (p_tcb_old->state != BLK_MSG){
        	p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        }
        k_tsk_switch(p_tcb_old);            // switch stacks
    }

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
	//puts the current task to the back of the ready queue
	int pos = gp_current_task->prio;
	//gp_current_task = prioq[pos];
	TCB *p_tcb = &g_tcbs[gp_current_task->tid];

	if (p_tcb->next != NULL){ //yield
		remove_from_prioq(gp_current_task->tid, pos);
		gp_current_task->next = NULL;
		add_to_prioq(p_tcb, pos);
	}

    return k_tsk_run_new();
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */
int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

    if(g_num_active_tasks == 160){
    	return RTX_ERR;
    }

    if (prio == PRIO_NULL){ //cant create one more NULL
        	return RTX_ERR;
    }

    int pos;
    /*
    zero the pos
     */
    for(pos = 0; pos < 160; pos++){
    	if(filled[pos] == 0){
    		break;
    	}
    }

    RTX_TASK_INFO p_taskinfo[1];

    //task = (task_t *)pos;

    //p_taskinfo[0].tid = *task;
    p_taskinfo[0].prio = prio;
    p_taskinfo[0].priv = 0;
    p_taskinfo[0].ptask = task_entry;
    p_taskinfo[0].k_stack_size = K_STACK_SIZE;
    p_taskinfo[0].u_stack_size = stack_size;



    TCB *p_tcb = &g_tcbs[pos];
    p_tcb->tid = pos;
    p_tcb->prio = prio;
    p_tcb->state = READY;
    p_tcb->func = task_entry;
    p_tcb->priv = 0;
	p_tcb->next = NULL;
	p_tcb->k_stack_size = K_STACK_SIZE;
	p_tcb->u_stack_size = stack_size;
    p_tcb->check = 0;
	p_tcb->k_stack_hi = (U32)k_alloc_k_stack(p_tcb->tid);

	//register setup

    if (k_tsk_create_new(p_taskinfo,p_tcb, pos) == RTX_OK) {
    	g_num_active_tasks++;
    }

	add_to_prioq(p_tcb, prio);
	filled[pos] = 1;
	if (gp_current_task->prio > prio) {
		k_tsk_yield();
	}

    return RTX_OK;

}

void k_tsk_exit(void) 
{
	//k_mem_print();

#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */
    k_mem_dealloc(gp_current_task->MB);
    task_t tid = gp_current_task->tid;
    remove_from_prioq(tid, gp_current_task->prio);
    gp_current_task->state = DORMANT;

    //mem dealloc
    if (gp_current_task->priv == 0) { //dealloc user stack
    	int error = k_mem_dealloc((void *) (gp_current_task->u_stack_hi-gp_current_task->u_stack_size));
    	//printf("dealloc at %d\n", gp_current_task->u_stack_hi-gp_current_task->u_stack_size);
    }
    g_num_active_tasks--;
    filled[tid] = 0;
    //run a new task
    k_tsk_run_new();
    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    if(prio < 1 || prio >254){// reserved tids
    	return RTX_ERR;
    }
    if(prio == PRIO_NULL){
    	return RTX_ERR;
    }

    if (g_tcbs[task_id].tid == 0){ //cant change null
    	return RTX_ERR;
    }

	if ((g_tcbs[task_id].priv == 1) && (gp_current_task->priv == 0)) {//user task cant change kernel task
		return RTX_ERR;
	}

	remove_from_prioq(task_id, g_tcbs[task_id].prio);
	g_tcbs[task_id].prio = prio;

	TCB *p_tcb = &g_tcbs[task_id];
	p_tcb->next = NULL;
	add_to_prioq(p_tcb, prio);

	if (task_id == gp_current_task->tid){
		TCB *highest = scheduler();
		if (highest->prio < prio) {
			k_tsk_yield();
		}
        return RTX_OK;
	}

    if (prio < gp_current_task->prio) {
		k_tsk_yield();
    }

    return RTX_OK;
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get_info: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL) {
        return RTX_ERR;
    }
    /* The code fills the buffer with some fake task information. 
       You should fill the buffer with correct information    */
    for (int i = 0; i < 160; i++) {
    	if (g_tcbs[i].tid == task_id && g_tcbs[i].func != NULL) {
    		buffer->tid = task_id;
			buffer->prio = g_tcbs[task_id].prio;
			buffer->state = g_tcbs[task_id].state;
			buffer->priv = g_tcbs[task_id].priv;
			buffer->u_stack_size = g_tcbs[task_id].u_stack_size;
			buffer->u_stack_hi = (g_tcbs[task_id].u_stack_hi + buffer->u_stack_size);
			buffer->k_stack_hi =   g_tcbs[task_id].k_stack_hi;
			buffer->k_stack_size = g_tcbs[task_id].k_stack_size;
			buffer->ptask = g_tcbs[task_id].func;
			return RTX_OK;
    	}
    }
    return RTX_ERR;
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    printf("k_tsk_get_tid: entering...\n\r");
#endif /* DEBUG_0 */
    task_t tid = gp_current_task->tid;
    return tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
