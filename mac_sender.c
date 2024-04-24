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
	
	
uint8_t calcuCheckSum(uint8_t* data, uint8_t length){
	uint8_t result = 0;
	for(int i = 0;i < length;i++){
		result += data[i];
	}
	return result;
}
	

void MacSender(void *argument)
{
	struct queueMsg_t message;
	uint8_t *data;
	
	const osMessageQueueAttr_t waitingQueueAttr = {
	.name = "WaitingQueue"
	};
	
	waitingQueueId = osMessageQueueNew(4,sizeof(struct queueMsg_t),&waitingQueueAttr);	
	
	while(true){
	osMessageQueueGet(queue_macS_id,&message,NULL,osWaitForever);
	data = message.anyPtr;
		
	switch(message.type){
		case NEW_TOKEN:
			sendToken();// send a new token
		break;
	  case START:
			gTokenInterface.connected = true;
			break;
		case STOP:
			gTokenInterface.connected = false;
			break;
		case DATA_IND:
		{
			//Create a data Frame with DATA_IND's data and put it in the wait message queue
			
			uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate some memory
			
			ptr[0] = (gTokenInterface.myAddress << 3) | message.sapi;	// control 1 (source Address + source SAPI)
			ptr[1] = (message.addr <<3) | message.sapi; 							//control 2 (dest. address + dest. SAPI)
			
			// copy the data and calculate length
			uint8_t length = 0;
			while(data[length] != '\0'){
				ptr[length+3] = data[length];
				length++;
			}
			ptr[length+3] = '\0'; // also put \0 in the end of the data
			
			ptr[2] = (length+1);// add length in the frame
			
			uint8_t checksum = calcuCheckSum(ptr,length+3);// calculate the checksum
			
			
			ptr[length+4] = checksum << 2;// status (checksum + Read bit + Ack bit)
			
			struct queueMsg_t msg;
			msg.anyPtr = ptr;
			msg.type = TO_PHY;
			
			osMessageQueuePut(waitingQueueId,&msg,osPriorityNormal,osWaitForever);
		}
			break;
		case TOKEN:
		{
			bool sendTokenList = false;// flage used to know if we have to send a tokenList message to the lcd
			for(int i = 0; i < TOKENSIZE;i++){ //Update station's available sapis
				if(gTokenInterface.station_list[i] != data[i+1]){
					gTokenInterface.station_list[i] = data[i+1];
					sendTokenList = true;
				}
			}
			if(sendTokenList){// if the token list has changed since the last time, we advertise the LCD by sending a TOKEN_LIST message
					struct queueMsg_t tokenlistMessage;
					tokenlistMessage.type = TOKEN_LIST;
					osMessageQueuePut(queue_lcd_id,&tokenlistMessage,NULL,osWaitForever);
			}
			
			struct queueMsg_t dataToSend;
			if(osMessageQueueGet(waitingQueueId,&dataToSend,NULL,0) == osOK){
				osMessageQueuePut(queue_phyS_id,&dataToSend,NULL,osWaitForever);
			}
			else{
				sendToken();
			}
			
			
		}
			break;
		case DATABACK:
		
			// when a message that we have send has made the turn => we need to check if it is Ack or not
			// if not Ack, send the message back in the wait queue. (3 times maximum)
		
		// TODO: check if message has been read or ack, if not resend another message => we need to make a copy of the message that we have sent
		{
			struct queueMsg_t dataToSend;
			if(osMessageQueueGet(waitingQueueId,&dataToSend,NULL,0) == osOK){
				osMessageQueuePut(queue_phyS_id,&dataToSend,NULL,osWaitForever);
			}
			else{
				sendToken();
			}		
		}			
			break;
	}
		osMemoryPoolFree(memPool,data);
}
}


