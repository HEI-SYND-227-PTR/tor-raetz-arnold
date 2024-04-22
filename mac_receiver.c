#include "main.h"
#include "stm32f7xx_hal.h"

void MacReceiver(void *argument)
{
	struct queueMsg_t message;
	uint8_t *data;
	while(true){

		osMessageQueueGet(queue_macR_id,&message,NULL,osWaitForever);
	
		data = message.anyPtr;
	
		if(data[0] == 0xFF){// if we received a token frame
			message.type = TOKEN;// change message type to token
			osMessageQueuePut(queue_macS_id,&message,osPriorityNormal,osWaitForever);// we transmit the token to the Mac sender
		}
		else{
				uint8_t mask = data[0] &0x78;
				if(mask == gTokenInterface.myAddress <<3)// if the address source is my address
				{
					// send DATABACK message
					message.type = DATABACK;
					osMessageQueuePut(queue_macS_id,&message,osPriorityNormal,osWaitForever);
				}
				else
					{
				osMemoryPoolFree(memPool,message.anyPtr);
				}
		}
		
	}
}
