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
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

/** 
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#ifdef DEBUG_0
 #include "printf.h"
#endif  /* DEBUG_0 */
#include "printf.h"


/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

//process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));


typedef struct __node_t{
	unsigned long addr;
	unsigned long size;
	struct __node_t *next;
	int allocated;
	task_t tid;
} node_t;

unsigned long node_size = sizeof(node_t);
unsigned long end_addr;
task_t current_tid;

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32* k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid+1];
}

U32* k_alloc_p_stack(task_t tid)
{
	current_tid = tid;
	TCB *p_tcb = &g_tcbs[tid];

	U32 * user_stack = k_mem_alloc(p_tcb->u_stack_size);

	//set stack address hi stuff
	p_tcb->u_stack_hi = (U32)user_stack +p_tcb->u_stack_size;
	return (U32*)(user_stack+p_tcb->u_stack_size);

	//return g_p_stacks[tid+1];
}

int k_mem_init(void) {
    end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
    //printf("Hi Vraj\n");
#ifdef DEBUG_0
    printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
    printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
#endif /* DEBUG_0 */
    if (end_addr % 8 != 0) {
    	end_addr += 8-(end_addr % 8);
    }

    unsigned long ram_end = RAM_END;
    if (ram_end % 8 != 0) {
    	ram_end -= (ram_end % 8);
    }
    node_t * head = (node_t *) end_addr;

    // SETTING UP HEAD FIELDS
    head->addr = end_addr+node_size;
	head->size = ram_end - end_addr;
	head->next = NULL;
	head->allocated = 0;

    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_alloc: requested memory size = %d\r\n", size);
#endif /* DEBUG_0 */

    if (size % 8 != 0) {
        	size += 8-(size % 8);
        }

    // SECTION to find space to allocate
    node_t *current = (node_t *) end_addr;
    while (1) {
    	if ((current->size > (size)) && (current->allocated == 0)) {
    		break;
    	}
    	if ((current->size == (size)) && (current->allocated == 0)) {
    		current->allocated = 1;
    		unsigned int * ptr = (unsigned int *)current->addr;
    		return ptr;
    	}
    	if (current->next == NULL) {
    		return NULL;
    	}
    	current = (node_t *) (current->next);

    }

   // created new used node to add to linked list
    node_t * new = (node_t *)(current->addr + (unsigned long)size);
    new->addr = current->addr + (unsigned long)size + node_size;
    new->size = current->size - ((unsigned long)size + node_size);
    new->next = current->next;
    new->allocated = 0;


    // changing current Node
    current->size = (unsigned long)size;
    current->next = new;
    current->allocated = 1;
    current->tid = current_tid;
    current_tid = NULL;
    if (current->tid == NULL) {
    	current->tid = gp_current_task->tid;
    }
    // returning ptr to required address
    unsigned int * ptr = (unsigned int *)current->addr;
    return ptr;
}

int k_mem_dealloc(void *ptr) {
#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);
#endif /* DEBUG_0 */

    if (ptr == NULL) {
    	return RTX_OK;
    }
    node_t *current = (node_t *) end_addr;

    while (1) {
    	if ( current->addr == (unsigned long) ptr && current->allocated == 1) {
    	    if (current->tid != gp_current_task->tid) {
    	        return RTX_ERR;
    	    }
    		break;
    	} else if (current->addr == (unsigned long) ptr && current->allocated == 0){
    		return RTX_ERR;
    	}
    	if (current->next == NULL) {
    		return RTX_ERR;
    	}
    	current = (node_t *) (current->next);
    }
    //found it
    current->allocated = 0;
    k_mem_coalesce();

    return RTX_OK;
}

void k_mem_coalesce(void) {
	node_t *current = (node_t *) end_addr;
	while(1) {
		if (current->next == NULL) {
			break;
		}
		if (current->allocated == 0) {
			if ((current->next)->allocated == 0) {
				current->size += (current->next)->size+sizeof(node_t);
				current->next = (current->next)->next;
			}
			else {
				current = (node_t *) (current->next);
			}
		}
		else {
			current = (node_t *) (current->next);
		}
	}
}

void k_mem_print(void) {
	node_t *current = (node_t *) end_addr;
	while(1) {
		if (current == NULL) {
			break;
		}//Current Header Start, Current Data Start, Allocated, Size, Next Header Start
		//printf(" The block of memory is: %u %u %d %u %u\n", (unsigned long) current, current->addr, current->allocated, current->size,current->next);
		current = (node_t *) (current->next);

	}
}

int k_mem_count_extfrag(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */
    int count = 0;
    node_t * current = (node_t *) end_addr;

    while(current->next != NULL) {
    	if ((current->size + node_size < size) && (current->allocated == 0)) {
    		count += 1;
    	}
    	current = (node_t *)(current->next);
    }

      return count;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
