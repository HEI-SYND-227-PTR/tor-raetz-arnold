#include "main.h"
#include "stm32f7xx_hal.h"


void MacSender(void *argument)
{
	queueMsg_t message;
	osMessageQueueGet(queue_macS_id,&message,osPriorityNormal,osWaitForever);
	uint8_t* msg
	
	switch(message.type){
		case NEW_TOKEN:
		{
			msg = osMemoryPoolAlloc(memPool,osWaitForever);
			queueMsg_t token;
			token.anyPtr = msg;
			msg[0] = TOKEN_TAG;
			msg[gTokenInterface.myAddress+1] = (1<<TIME_SAPI) + gTokenInterface.connected?(1<<CHAT_SAPI):0;
			osMessageQueuePut(queue_phyS_id,&token,osWaitForever);
		}		
		break;
	  case START:
			gTokenInterface.connected = true;
			break;
		case STOP:
			gTokenInterface.connected = false;
			break;
		case DATA_IND:
			// TODO: put the data in a queue and wait to send the message
			break;
		case TOKEN:
			// TODO: send all the data in the queue 
			// and send token??
			break;
		case DATABACK:
			// TODO: put back the data in the queue and count (maximum 3 databack)
			break;
}
