/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "printf.h"
#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

void cpy_message(void * src, void * dest) {
	char* csrc = (char*)src;

	char *cdest = (char *)dest;
	for (int i = 0; i < ((RTX_MSG_HDR *)src)->length; i++){
		cdest[i] = csrc[i];
	}
}

void cpy_to_arr(const RTX_MSG_HDR * src, msg_queue * q) {
	int len = src->length;
	char* csrc = (char*)src;
	for (int i = q->tail ; i < (q->tail + len); i++){
		int j = i;
		if (i >= q->size) {
			j = (i % q->size);
		}
		q->array[j] = csrc[i-(q->tail)];
	}
}

void cpy_from_arr(RTX_MSG_HDR * dest, const msg_queue * q) {
	char* cdest = (char*)dest;
	for (int i = q->head ; i < (q->head + 4); i++) {
		int j = i;
		if (i >= q->size) {
			j = (i % q->size);
		}
		cdest[i-(q->head)] = q->array[j];
	}
	int len = dest->length;
	for (int i = (q->head + 4) ; i < (q->head + len); i++){
		int j = i;
		if (i >= q->size) {
			j = (i % q->size);
		}
		cdest[i-(q->head)] = q->array[j];
	}
}

void msg_queue_init(msg_queue *q, unsigned long start, size_t size) {
	q->tail = 0;
	q->head = 0;
	q->start_addr = start;
	q->end_addr = start + size;
	q->total_msgs = 0;
	q->size = size;
	q->array = (char *)(start + sizeof(msg_queue));
}

int msg_queue_enqueue(msg_queue *q, RTX_MSG_HDR *msg) {
	if(q->total_msgs == 0){
		cpy_to_arr(msg, q);
		q->tail += msg->length;
	}
	else {
		cpy_to_arr(msg, q);
		q->tail = ((q->tail + msg->length) % q->size);
	}
	q->total_msgs += 1;
	return 0;
}

int msg_queue_dequeue(msg_queue *q,  RTX_MSG_HDR *msg) {
	if(q->total_msgs == 0){
		//printf("The mailbox is empty, no messages can be retrieved\n");
		return NULL;
	}
	else if(q->total_msgs == 1){
		cpy_from_arr(msg, q);
		q->head = 0;
		q->tail = 0;
		q->total_msgs -= 1;
	}
	else{
		cpy_from_arr(msg, q);
		q->head = ((msg->length + q->head) % q->size);
		q->total_msgs -= 1;
	}
	return 1;
}




//we can move this later

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */
    // size is less then minimum size for mailbox
    if (size < MIN_MBX_SIZE){
    	return RTX_ERR;
    }
    //max 1 mailbox per task
    if (gp_current_task->check == 1){
    	return RTX_ERR;
    }
    mailbox * newmb = k_mem_alloc(sizeof(mailbox) + sizeof(msg_queue) + size);
    newmb->size = size;
    newmb->available_size = size;

    msg_queue_init(&(newmb->q),(unsigned long)&(newmb->q), size);

    gp_current_task->check = 1;
    gp_current_task->MB = newmb;

    return RTX_OK;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
    if ((&g_tcbs[receiver_tid] == NULL) || (g_tcbs[receiver_tid].state == DORMANT)){
    	return RTX_ERR;
    }
    TCB *receiver_task = &g_tcbs[receiver_tid];

    if (receiver_task->check == 0){
    	return RTX_ERR;
    } //no mailbox

    if ((buf == NULL)) {
    	return RTX_ERR;
    } // no buffer

    if (((RTX_MSG_HDR*) buf)->length < MIN_MSG_SIZE){
    	return RTX_ERR;
    } // message too small

    if (receiver_task->MB->available_size < ((RTX_MSG_HDR*) buf)->length){
    	return RTX_ERR;
    } //full receiving mailbox

    if (((RTX_MSG_HDR*)  buf)->sender_tid!= TID_UART_IRQ){
    	((RTX_MSG_HDR*)  buf)->sender_tid = gp_current_task->tid;
    }

    if (msg_queue_enqueue(&(receiver_task->MB->q), (RTX_MSG_HDR*)  buf) == RTX_OK){
    	if (receiver_task->state == BLK_MSG) {
        	receiver_task->MB->available_size -= ((RTX_MSG_HDR*) buf)->length;
        	receiver_task->state = READY;
			if (receiver_task->prio < gp_current_task->prio) { //revisit spec
				k_tsk_yield();
			}
    	}
    	else{
        	receiver_task->MB->available_size -= ((RTX_MSG_HDR*) buf)->length;
    	}
	} else {
		// If the enqueue operation failed, return an error
		return RTX_ERR;
	}
    return RTX_OK;


}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    if (gp_current_task->check == 0){
 	   return RTX_ERR; //no mailbox
    }
    if (buf == NULL){
 	   return RTX_ERR; //null buf
    }


    if (gp_current_task->MB->available_size == gp_current_task->MB->size){ // empty
 	   gp_current_task->state = BLK_MSG; //block this
 	   k_tsk_run_new();
    }

    if (gp_current_task->MB->q.total_msgs == 0) {
  	   gp_current_task->state = BLK_MSG; //block this
  	   k_tsk_run_new();
    }
    msg_queue_dequeue(&(gp_current_task->MB->q), (RTX_MSG_HDR*) buf);
    if ((RTX_MSG_HDR*)buf == NULL) {
 	   return RTX_ERR;
    }
    gp_current_task->MB->available_size += ((RTX_MSG_HDR*)buf)->length;

    if (((RTX_MSG_HDR*)buf)->length > len ){ // not enough memory allocated

 	   //the top message is discarded anyways
 	   return RTX_ERR;
    }

    if ( sender_tid != NULL){
 	   *(sender_tid) = ((RTX_MSG_HDR*)buf)->sender_tid;
    }
     return RTX_OK;


}

int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    if (gp_current_task->check == 0){
 	   return RTX_ERR; //no mailbox
    }
    if (buf == NULL){
 	   return RTX_ERR; //null buf
    }


    if (gp_current_task->MB->available_size == gp_current_task->MB->size){ //full
 	   //gp_current_task->state = BLK_MSG; //block this
 	   k_tsk_run_new();
    }

    RTX_MSG_HDR* received_msg = NULL;
    msg_queue_dequeue(&(gp_current_task->MB->q), (RTX_MSG_HDR*) buf);
    if ((RTX_MSG_HDR*)buf == NULL) {
 	   return RTX_ERR;
    }
    gp_current_task->MB->available_size += ((RTX_MSG_HDR*)buf)->length;

    if (((RTX_MSG_HDR*)buf)->length > len ){ // not enough memory allocated

 	   //the top message is discarded anyways
 	   return RTX_ERR;
    }

    if ( sender_tid != NULL){
 	   *(sender_tid) = received_msg->sender_tid;
    }
     return RTX_OK;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
