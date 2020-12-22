#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <string.h>

// add any #defines here

// add global variables here
bool* GeneralLoyal;
uint8_t GeneralReporter;
osMessageQueueId_t msgQueue[3][7];
osStatus_t status;
uint8_t n,m;
osSemaphoreId_t sem;


/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	
	// initialized m and n
	m = 0;
	n = nGeneral;
	// Count number of traitors (m)
	for (uint8_t i = 0; i<n; i++) {
		if(!loyal[i]) 
			m++;
	}
	// Used c_assert to print n is not > 3*m
	if (!c_assert(n > 3*m))
		return false;
	
	// Initialize GeneralLoyal and GeneralReporter
	GeneralLoyal = loyal;
	GeneralReporter = reporter;
	
	// Initialized message Queues with max possible capacity + 1, and size of items 8*char(-:-:-:-\0)
	for (uint8_t i = 0; i < nGeneral; i++){
		msgQueue[0][i] = osMessageQueueNew(31, 8*sizeof(char), NULL);
		if (!c_assert(msgQueue[0][i] != NULL))
			return false;
		msgQueue[1][i] = osMessageQueueNew(7, 8*sizeof(char), NULL);
		if (!c_assert(msgQueue[1][i] != NULL))
			return false;
		msgQueue[2][i] = osMessageQueueNew(2, 8*sizeof(char), NULL);
		if (!c_assert(msgQueue[2][i] != NULL))
			return false;
	}
	
	//initalize semophore
	sem = osSemaphoreNew(6,0,NULL);
	if (!c_assert(sem != NULL))
			return false;
	
	return true;
}

/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	// Delete message Queue
	for (uint8_t m = 0; m < 3; m++){
		for (uint8_t i = 0; i < n; i++){
			status = osMessageQueueDelete(msgQueue[m][i]);
			if (c_assert(status == osOK)){
				msgQueue[m][i] = NULL;
			}
		}
	}
	
	// Delete semophore
	status = osSemaphoreDelete(sem);
	if (c_assert(status == osOK)){
		sem = NULL;
	}
	
	// Set Global pointer = NULL
	GeneralLoyal = NULL;
}


/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t commander) {
	// If Commander is Loyal
	if(GeneralLoyal[commander] == true) {
		// Create message
		char cmdr[3];
		sprintf(cmdr,"%d",commander);
		char msg[8] = "";
		strcat(msg,cmdr);
		strcat(msg,":");
		strcat(msg,&command);
		// Send message to Message Queue 2 of each commander
		for(uint8_t i =0; i<n; i++){
			// Skip sending to itself
			if (i != commander) 
				osMessageQueuePut(msgQueue[2][i],msg, 0, 0);
		}
	}
	
	// If Commander is traitor
	else{
		// Create message
		char cmdr[3];
		sprintf(cmdr,"%d",commander);
		char msg[8] = "";
		strcat(msg,cmdr);
		strcat(msg,":");
		// Send message to Message Queue 2 of each commander
		for(uint8_t i =0; i<n; i++) {
			// Skip sending to itself
			if (i == commander) 
				continue;
			
			// if leutenent is even send R
			else if (i%2 == 0){
				char temp[8] = "";
				strcpy(temp,msg);
				strcat(msg,"R");
				osMessageQueuePut(msgQueue[2][i],msg, 0, 0);
				strcpy(msg,temp);
			}
			
			// if leutenent is even send R
			else{
				char temp[8] = "";
				strcpy(temp,msg);
				strcat(msg,"A");
				osMessageQueuePut(msgQueue[2][i],msg, 0, 0);
				strcpy(msg,temp);
			}
		}
	}
	
	// Wait for n-1 generals to finish running
	for(uint8_t i = 0; i<(n-1); i++)
		osSemaphoreAcquire(sem,osWaitForever);
	
	printf("\n");
}

void OM(uint8_t level, char msg_OM[8], uint8_t id) {
	// convert ID to character string
	char id_char[3];
	sprintf(id_char,"%d",id);
	// variable for counting num of messages sent and message recieved 
	uint8_t msg_count = 0;
	char msg_recieved[8];
	
	// if last level (0) Print or exit
	if (level == 0){
		// Check if ID = Reporter
		if(id == GeneralReporter){
			// Leave out printing messages from reporter itself
			if(strstr(msg_OM,id_char) == NULL){
				printf(" %s ",msg_OM);
				return;
			}
		}
		// Exit if not printing
		else {
			return;
		}
	}
	
	// If level > 0
	else{
		// Create Message
		char msg[8] = "";
		strcat(msg,id_char);
		strcat(msg,":");
		strcat(msg,msg_OM);
		
		// if Leutinent is loyal
		if (GeneralLoyal[id]){
			// send message recieved to rest
			for(uint8_t i=0; i<n; i++){
				// Convert iterator i to char string
				char i_char[3];
				sprintf(i_char,"%d",i);
				// If message does not contain iterator send to them
				if(strstr(msg,i_char) == NULL){
					osMessageQueuePut(msgQueue[level-1][i],msg,0,0);
					msg_count++;
				}
			}
		}
		
		// If Leutinent is Traitor
		else {
			// Change message such if leutinent id even R else A
			int size = strlen(msg);
			if(id%2==0)
				msg[size-1] = 'R';
			else
				msg[size-1] = 'A';
			
			//send message to rest
			for(uint8_t i=0; i<n; i++){
				// Convert iterator i to char string
				char i_char[3];
				sprintf(i_char,"%d",i);
				// If message does not contain iterator send to them
				if(strstr(msg,i_char) == NULL){
					osMessageQueuePut(msgQueue[level-1][i],msg,0,0);
					msg_count++;
				}
			}
		}
		
		// Recieve num of messages = num of messages sent
		for(uint8_t i=msg_count; i>0; i--){
			osMessageQueueGet(msgQueue[level-1][id],msg_recieved, NULL, osWaitForever);
			// Pass message recieved to inner level with same ID
			OM((level-1),msg_recieved,id);
		}
	}
}

/** Generals are created before each test and deleted after each
  * test.  The function should wait for a value from broadcast()
  * and then use the OM algorithm to solve the Byzantine General's
  * Problem.  The general designated as reporter in setup()
  * should output the messages received in OM(0).
  * idPtr: pointer to general's id number which is in [0,n-1]
  */
void general(void *idPtr) {
	uint8_t id = *(uint8_t *)idPtr;
	// declare variable and recieve message from commander
	char msg_recieved[8];
	osMessageQueueGet(msgQueue[2][id],msg_recieved,NULL,osWaitForever);
	// Start recursive function with Level m(num of traitors), message recieved and ID of general invoking
	OM(m,msg_recieved,id);
	
	// Wait for Recursive function to return and send signal to broadcast that general finished exicution
	osSemaphoreRelease(sem);
	return;
}
