#include "main.h"
#include "stm32f7xx_hal.h"

	osMessageQueueId_t waitingQueueId;
	void sendToken(){
			uint8_t* token = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory to create new token		

			for(int i = 0; i < TOKENSIZE;i++){
				token[i+1] = gTokenInterface.station_list[i];
			}
			
			struct queueMsg_t msg;// message to send
			msg.anyPtr = token;
			msg.type = TO_PHY;
			token[0] = TOKEN_TAG; // put the token tag in the first byte
			token[gTokenInterface.myAddress+1] = (1<<TIME_SAPI) + gTokenInterface.connected?(1<<CHAT_SAPI):0;// send our available sapi
			osMessageQueuePut(queue_phyS_id,&msg,osPriorityNormal,osWaitForever);// put the token message in the queue physic Send
	}
	
	

void MacSender(void *argument)
{
	struct queueMsg_t message;
	uint8_t *data;
	
	const osMessageQueueAttr_t waitingQueueAttr = {
	.name = "WaitingQueue"
	};
	
	waitingQueueId = osMessageQueueNew(2,sizeof(struct queueMsg_t),&waitingQueueAttr);	
	
	while(true){
	osMessageQueueGet(queue_macS_id,&message,NULL,osWaitForever);
	data = message.anyPtr;
		
	switch(message.type){
		case NEW_TOKEN:
			sendToken();
		break;
	  case START:
			gTokenInterface.connected = true;
			break;
		case STOP:
			gTokenInterface.connected = false;
			break;
		case DATA_IND:
			// TODO: put the data in a queue and wait to send the message
			// check if the destination's SAPI is active or not
			break;
		case TOKEN:
		{
			bool sendTokenList = false;
			for(int i = 0; i < TOKENSIZE;i++){
				if(gTokenInterface.station_list[i] != data[i+1]){
					gTokenInterface.station_list[i] = data[i+1];
					sendTokenList = true;
				}
			}
			if(sendTokenList){
					struct queueMsg_t tokenlistMessage;
					tokenlistMessage.type = TOKEN_LIST;
					osMessageQueuePut(queue_lcd_id,&tokenlistMessage,NULL,osWaitForever);
			}
			
			// TODO: send messages
			
			sendToken();
		}
			break;
		case DATABACK:// when a message that we have send has made the turn => we need to check if it is Ack or not
			// if not Ack, send the message back in the wait queue. (3 times maximum)
			break;
	}
		osMemoryPoolFree(memPool,message.anyPtr);
}
}

