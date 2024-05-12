/* The KCD Task Template File */
#include "Serial.h"
#include "k_rtx.h"
#include "common.h"
#include "ae.h"

//SET BAUD RATE TO 115200

//replace this structure with something better to keep track of TID -> cmd mapping
//This is a mock for how the KCD_TASK should be working
//replace this with a linked list or smth im lazy now
typedef struct cmd_map{
    char cmd;
    int tid;
}cmd_map;

cmd_map g_cmd_mappings[255]; //arbitrary design for keeping track of registered tasks and their cmds
//probably not max 20 cmds
//(REDO THIS DATA STRUCTURE)
task_t sender_tid = 0; // tid of the task that sent request
task_t registered_tid = 0; // tid of the task registered with cmd
char cmd_buf[64] = {0}; // Command buffer
int cmd_buf_index = 0;
int cmd_flag = 0;


void kcd_task(void)
{
    /* not implemented yet */
	//create mail_box
	mbx_create(KCD_MBX_SIZE);

	RTX_MSG_HDR *msg = mem_alloc(sizeof(RTX_MSG_HDR) + 1);

	RTX_MSG_HDR *sent_command = mem_alloc(sizeof(RTX_MSG_HDR) + 64); //sending msg cmd_buf_index

	for (int i = 0; i < 255; i++){
		g_cmd_mappings[i].tid = -1;
	}

	while(1) { //infinite loop to receive msg
		//
		//printf("whiling\n");
		if (recv_msg(&sender_tid, msg, sizeof(RTX_MSG_HDR) + 4) == RTX_OK) {
			if ((msg->type == KCD_REG) && (msg->length ==sizeof(RTX_MSG_HDR) + 1) ){ // command registration

				char cmd_identifier = *((char *)(msg + 1)); // W

				for (int i = 0; i < 255; i++){
					if (g_cmd_mappings[i].cmd == cmd_identifier){ //UPDATING PREVIOUS COMMAND
						g_cmd_mappings[i].tid = sender_tid;
						/*printf("g_cmd_mappings[i].cmd: %c \n", g_cmd_mappings[i].cmd);
						printf("g_cmd_mappings[i].tid: %d \n", g_cmd_mappings[i].tid);*/
						break;
					}
					if (g_cmd_mappings[i].tid == -1){ // REGISTERING NEW CMD
						g_cmd_mappings[i].cmd = cmd_identifier;
						g_cmd_mappings[i].tid = sender_tid;

						/*printf("g_cmd_mappings[i].cmd: %c \n", g_cmd_mappings[i].cmd);
						printf("g_cmd_mappings[i].tid: %d \n", g_cmd_mappings[i].tid);*/
						break;

					}

				}


				//register the cmd to TID

			}else if (msg->type == KEY_IN && sender_tid == TID_UART_IRQ){ // terminal keyboard input
				char c = *((char*)(msg + 1));

				if (c == '%'){
					cmd_flag = 1;
				}

				//printf("%c", c);
				if (cmd_flag){
					if (c == 13){ //enter
						SER_PutStr(1, "\n\r");
						//if ((c == '\r') || (c == '\n')){				// enter was pressed
						if (cmd_buf_index <= 64){ //command of size less than 64B was passed - valid command

							sent_command->length = sizeof(RTX_MSG_HDR)+cmd_buf_index-1; //+1
							sent_command->type = KCD_CMD;


							for (int i = 0; i < 255; i++){ //look up registered tid to this cmd (REDO THIS DATA STRUCTURE)
								if (g_cmd_mappings[i].cmd == cmd_buf[1]){
									registered_tid = g_cmd_mappings[i].tid;
									/*printf("registered_tid of %c is %d\n", g_cmd_mappings[i].cmd, registered_tid);*/
									break;
								}
							}
							if (registered_tid == 0){ //unregistered command reject
								SER_PutStr (1,"Command cannot be processed");
								cmd_buf_index = 0;
								registered_tid = 0;
								cmd_flag = 0;
								continue; //new lab
							}

							//copy into msg
							char *csrc = (char*)cmd_buf;
							char *cdest = (char *)(sent_command + 1);
							for (int i = 0; i < cmd_buf_index-1; i++){
								cdest[i] = csrc[i+1]; //need to exclude the %
							}

							if (send_msg(registered_tid, sent_command) == RTX_OK){
								// send cmd to registered TID
								/*printf("SENT LENGTH %d :", sent_command->length);
								for (int j = 0; j < sent_command->length-sizeof(RTX_MSG_HDR); j++) {
									char c = *((char *)(sent_command + 1) + j);
									printf("%c", c);
								}
								printf("\n"); */
							}
							else{
								SER_PutStr (1,"Command cannot be processed");
							}
						}
						else{ //invalid command
							SER_PutStr (1,"Invalid Command");
						}

						//clean out index to 0
						cmd_buf_index = 0;
						registered_tid = 0;
						cmd_flag = 0;

						//maybe dont need to clean out buffer, if we just read up to buff_index
					}
					else{//different character was pressed
						if (cmd_buf_index < 64){
							cmd_buf[cmd_buf_index] = c;
							/*printf("\ncmd:");

							for (int i = 0; i < cmd_buf_index; i++){
								printf("%c",cmd_buf[i]);
							}
							printf("\n");*/

						}
						cmd_buf_index++; //outside, we want to keep track of this to tell if request message is too long
						//printf("the current length is: %d\n", cmd_buf_index);
					}
				}
				else if (c == 13){ //non % cmd
					SER_PutStr(1, "\n\r");
					SER_PutStr (1,"Invalid Command");

				}
			}
		}

	}
}
/*
	 * [0] = %, command identifier
	 * [1] = char that identifies the cmd
	 * [2-63] = data for the cmd
	 */
