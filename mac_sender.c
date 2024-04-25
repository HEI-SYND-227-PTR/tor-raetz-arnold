#include "main.h"
#include "stm32f7xx_hal.h"


	osMessageQueueId_t waitingQueueId; // waiting queue used to store all the DATA_IND message before we can send them
	
	
	// this function send a token frame when called.
	void sendToken(){
			uint8_t* token = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory to create new token		

			for(int i = 0; i < TOKENSIZE;i++){// get all the stations's SAPI state and put them in the frame
				token[i+1] = gTokenInterface.station_list[i];
			}
			
			struct queueMsg_t msg;// message to send
			msg.anyPtr = token;
			msg.type = TO_PHY;
			token[0] = TOKEN_TAG; // put the token tag in the first byte
			token[gTokenInterface.myAddress+1] = (1<<TIME_SAPI) + gTokenInterface.connected?(1<<CHAT_SAPI):0;// send our SAPI's state
			osMessageQueuePut(queue_phyS_id,&msg,osPriorityNormal,osWaitForever);// put the token message in the queue physic Send
	}
	
// function used to calculate the checksum of the data putted in entry	
uint8_t calcuCheckSum(uint8_t* data, uint8_t length){
	uint8_t result = 0;
	for(int i = 0;i < length;i++){
		result += data[i];
	}
	return result;
}
	
// function used to send an error to the LCD when we can't send a message to a specific station
void sendMacError(){
		struct queueMsg_t msg;// message to send
		uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory 
		ptr = "Error, could not send message";// message for the lcd
		msg.anyPtr = ptr;
		msg.type = MAC_ERROR;
		osMessageQueuePut(queue_lcd_id,&msg,NULL,osWaitForever);//send message
}

void MacSender(void *argument)
{
	struct queueMsg_t macSmessage;
	uint8_t *data;
	
	const osMessageQueueAttr_t waitingQueueAttr = {
	.name = "WaitingQueue"
	};
	waitingQueueId = osMessageQueueNew(4,sizeof(struct queueMsg_t),&waitingQueueAttr);	
	
	uint8_t nResend = 0;// used to count how many times we have resent a message non acknowledged
	
	while(true){
	osMessageQueueGet(queue_macS_id,&macSmessage,NULL,osWaitForever);
	data = macSmessage.anyPtr;
		
	switch(macSmessage.type){
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
			
			ptr[0] = (gTokenInterface.myAddress << 3) | macSmessage.sapi;	// control 1 (source Address + source SAPI)
			ptr[1] = (macSmessage.addr <<3) | macSmessage.sapi; 							//control 2 (dest. address + dest. SAPI)
			
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
			for(int i = 0; i < 15;i++){ //Update station's available sapis
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
		{
			struct queueMsg_t dataToSend;
			uint8_t dataLength = data[2];
		
		if(((data[dataLength+3] & 0x3) != 0x3) && (nResend < 2) && ((data[1] & 0x78) != (0xF <<3))){// check read and ack bit, check nResend (must not be = 2), check that it is not a broadcast frame
			// if the message has not be acknowledged, we need to resend it
			nResend ++;
			data[dataLength+3] = (data[dataLength+3] & 0xFC); // reset read and ack bits
			uint8_t *ptr = osMemoryPoolAlloc(memPool,osWaitForever);
			memcpy(ptr,data,dataLength+4);
			macSmessage.anyPtr = ptr;
			osMessageQueuePut(queue_phyS_id,&macSmessage,NULL,osWaitForever);
		}
		else{
				if(nResend == 2){
					sendMacError();
				}
				nResend = 0;// reset the counter
				if(osMessageQueueGet(waitingQueueId,&dataToSend,NULL,0) == osOK){//check if there is messages to send
					osMessageQueuePut(queue_phyS_id,&dataToSend,NULL,osWaitForever);// if yes, send it to physic sender
				}
				else{
					sendToken();// if no more messages to send, send the token to the next station
				}	
			}				
		}			
			break;
	}
		osMemoryPoolFree(memPool,data);// release the memory
}
}


