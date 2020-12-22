#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <string.h>

// add any #defines here

// add global variables here
bool* GeneralLoyal;
uint8_t GeneralReporter;
uint8_t GeneralCommander;
osMessageQueueId_t msgQueue[3][7];
osStatus_t status;
uint8_t n,m;
osSemaphoreId_t sem;

uint8_t barrier_count[2];
osMutexId_t barrier_mut[2];
osSemaphoreId_t barrier_sem[2];


/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	
	m = 0;
	n = nGeneral;
	for (uint8_t i = 0; i<n; i++) {
		if(!loyal[i]) 
			m++;
	}
	if (!c_assert(n > 3*m))
		return false;
	
	GeneralLoyal = loyal;
	GeneralReporter = reporter;
	
	for (uint8_t i = 0; i < nGeneral; i++){
		msgQueue[0][i] = osMessageQueueNew(21, 8*sizeof(char), NULL);
		if (!c_assert(msgQueue[0][i] != NULL))
			return false;
		msgQueue[1][i] = osMessageQueueNew(7, 8*sizeof(char), NULL);
		if (!c_assert(msgQueue[1][i] != NULL))
			return false;
		msgQueue[2][i] = osMessageQueueNew(2, 8*sizeof(char), NULL);
		if (!c_assert(msgQueue[2][i] != NULL))
			return false;
	}
	
	sem = osSemaphoreNew(2,0,NULL);
	
	for(uint8_t bar=0; bar<2; bar++){
		barrier_count[bar] = 0;
		barrier_mut[bar] = osMutexNew(NULL);
		barrier_sem[bar] = osSemaphoreNew(2,0,NULL);
	}
	
	return true;
}

/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	for (uint8_t m = 0; m < 3; m++){
		for (uint8_t i = 0; i < n; i++){
			status = osMessageQueueDelete(msgQueue[m][i]);
			if (c_assert(status == osOK)){
				msgQueue[m][i] = NULL;
			}
		}
	}

	osSemaphoreDelete(sem);
	GeneralLoyal = NULL;
	
	for(uint8_t bar=0; bar<2; bar++){
		barrier_count[bar] = 0;
		osMutexDelete(barrier_mut[bar]);
		osSemaphoreDelete(barrier_sem[bar]);
	}
	
}


/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t commander) {
	if(GeneralLoyal[commander] == true) {
		char cmdr[3];
		sprintf(cmdr,"%d",commander);
		char msg[8] = "";
		strcat(msg,cmdr);
		strcat(msg,":");
		strcat(msg,&command);
		for(uint8_t i =0; i<n; i++){
			if (i == commander) 
				continue;
		
			else {
				osMessageQueuePut(msgQueue[2][i],msg, 0, 0);
			}
		}
	}
	
	else{
		char cmdr[3];
		sprintf(cmdr,"%d",commander);
		char msg[8] = "";
		strcat(msg,cmdr);
		strcat(msg,":");
		for(uint8_t i =0; i<n; i++) {
			if (i == commander) 
				continue;
		
			else if (i%2 == 0){
				char temp[8] = "";
				strcpy(temp,msg);
				strcat(msg,"R");
				osMessageQueuePut(msgQueue[2][i],msg, 0, 0);
				strcpy(msg,temp);
			}
			
			else{
				char temp[8] = "";
				strcpy(temp,msg);
				strcat(msg,"A");
				osMessageQueuePut(msgQueue[2][i],msg, 0, 0);
				strcpy(msg,temp);
			}
		}
	}
	
	for(uint8_t i = 0; i<(n-1); i++)
		osSemaphoreAcquire(sem,osWaitForever);
	
	printf("\n");
}

void barrier(uint8_t c){
	int max = (n-1);
	if (m==2 && c==0)
		max = (n-1)*(n-2);
	osMutexAcquire(barrier_mut[c],osWaitForever);
	barrier_count[c]++;
	if(barrier_count[c] == max)
		osSemaphoreRelease(barrier_sem[c]);
	osMutexRelease(barrier_mut[c]);
	
	osSemaphoreAcquire(barrier_sem[c],osWaitForever);
	osSemaphoreRelease(barrier_sem[c]);
	
	osMutexAcquire(barrier_mut[c],osWaitForever);
	barrier_count[c]--;
	if(barrier_count[c] == 0)
		osSemaphoreAcquire(barrier_sem[c],osWaitForever);
	osMutexRelease(barrier_mut[c]);	
}

void OM(uint8_t level, char msg_OM[8], uint8_t id) {
	char id_char[3];
	sprintf(id_char,"%d",id);
	uint8_t msg_count = 0;
	char msg_recieved[8];
	
	if (level == 0){
		if(id == GeneralReporter){
			if(strstr(msg_OM,id_char) == NULL){
				printf(" %s ",msg_OM);
				return;
			}
		}
		else {
			return;
		}
	}
	
	else{
		char msg[8] = "";
		strcat(msg,id_char);
		strcat(msg,":");
		strcat(msg,msg_OM);
		
		if (GeneralLoyal[id]){
			for(uint8_t i=0; i<n; i++){
				char i_char[3];
				sprintf(i_char,"%d",i);
				if(strstr(msg,i_char) == NULL){
					osMessageQueuePut(msgQueue[level-1][i],msg,0,0);
					msg_count++;
				}
			}
		}
		
		else {
			int size = strlen(msg);
			if(id%2==0)
				msg[size-1] = 'R';
			else
				msg[size-1] = 'A';
			
			for(uint8_t i=0; i<n; i++){
				char i_char[3];
				sprintf(i_char,"%d",i);
				if(strstr(msg,i_char) == NULL){
					osMessageQueuePut(msgQueue[level-1][i],msg,0,0);
					msg_count++;
				}
			}
		}
		
		//barrier(level-1);
		
		for(uint8_t i=msg_count; i>0; i--){
			osMessageQueueGet(msgQueue[level-1][id],msg_recieved, NULL, osWaitForever);
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
	char msg_recieved[8];
	osMessageQueueGet(msgQueue[2][id],msg_recieved,NULL,osWaitForever);
	OM(m,msg_recieved,id);
		
	osSemaphoreRelease(sem);
	return;
}