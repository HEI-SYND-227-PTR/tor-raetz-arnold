#include "main.h"
#include "stm32f7xx_hal.h"
#include <string.h> 

	osMessageQueueId_t waitingQueueId; // waiting queue used to store all the DATA_IND message before we can send them
	
	
	// this function send a token frame when called.
	void sendToken(){
			uint8_t* token = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory to create new token		

			for(int i = 0; i < 15;i++){// get all the stations's SAPI state and put them in the frame
				token[i+1] = gTokenInterface.station_list[i];
			}
			
			struct queueMsg_t msg;// message to send
			msg.anyPtr = token;
			msg.type = TO_PHY;
			token[0] = TOKEN_TAG; // put the token tag in the first byte
			
			token[gTokenInterface.myAddress+1] = (1<<TIME_SAPI);
			if(gTokenInterface.connected){
				token[gTokenInterface.myAddress+1] |= (1<<CHAT_SAPI);
			}
			
			osMessageQueuePut(queue_phyS_id,&msg,osPriorityNormal,osWaitForever);// put the token message in the queue physic Send
	}
	
	// this function send a new token frame in the ring
	void sendNewToken(){
		uint8_t* token = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory to create new token		

			for(int i = 0; i < 15;i++){// reset memory to 0
				token[i+1] = 0;
			}
			
			struct queueMsg_t msg;// message to send
			msg.anyPtr = token;
			msg.type = TO_PHY;
			token[0] = TOKEN_TAG; // put the token tag in the first byte
			
			token[gTokenInterface.myAddress+1] = (1<<TIME_SAPI);
			token[gTokenInterface.myAddress+1] |= (1<<CHAT_SAPI);// by default, I say that chat SAPI is connected

			
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
		uint8_t* ptr = NULL;
		uint8_t* message = "Error, could not send message\n";
	
		ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory 
		if(ptr != NULL){	
			memcpy(ptr,message,31);	// copy the message in ptr
			msg.anyPtr = ptr;
			msg.type = MAC_ERROR;
			osMessageQueuePut(queue_lcd_id,&msg,osPriorityNormal,osWaitForever);//send message
		}
}

// this function sends all the data in the waiting queue and when there is no more data to send, it calls the sendtoken funcion
void sendData(){
		struct queueMsg_t dataToSend;
		if(osMessageQueueGet(waitingQueueId,&dataToSend,(uint8_t*)osPriorityNormal,0) == osOK){
			
		uint8_t* ptr = dataToSend.anyPtr;
			
			osMessageQueuePut(queue_phyS_id,&dataToSend,osPriorityNormal,osWaitForever);
		}
		else{
			sendToken();
		}
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
	osMessageQueueGet(queue_macS_id,&macSmessage,(uint8_t*)osPriorityNormal,osWaitForever);
	data = macSmessage.anyPtr;
		
	switch(macSmessage.type){
		case NEW_TOKEN:
			sendNewToken();// send a new token
		break;
	  case START:
			gTokenInterface.connected = true;
			gTokenInterface.station_list[gTokenInterface.myAddress] = gTokenInterface.station_list[gTokenInterface.myAddress] | (1<<CHAT_SAPI);//set chat SAPI's bit
			break;
		case STOP:
			gTokenInterface.connected = false;
			gTokenInterface.station_list[gTokenInterface.myAddress] = (gTokenInterface.station_list[gTokenInterface.myAddress] & 0xFD);//reset chat SAPI's bit
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
			
					
			//osMessageQueuePut(waitingQueueId,&msg,osPriorityNormal,osWaitForever);
			if(osMessageQueuePut(waitingQueueId,&msg,osPriorityNormal,0) != osOK){// if we can't put the message in the queue
				osMemoryPoolFree(memPool,data);//release memory
			}
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
					osMessageQueuePut(queue_lcd_id,&tokenlistMessage,osPriorityNormal,osWaitForever);
			}
			sendData();			
			
		}
			break;
		case DATABACK:
		{
			struct queueMsg_t dataToSend;
			uint8_t dataLength = data[2];
			uint8_t readAck = (data[dataLength+3] & ACK_READ_MASK);
			uint8_t destAddress = ((data[1] & ADDRESS_MASK)>>3);
		if((readAck != 0x3) && (nResend < 2) && (destAddress != 0xF)){// check read and ack bit, check nResend (must not be = 2), check that it is not a broadcast frame
			// if the message is not acknowledged
			nResend ++;
			data[dataLength+3] = (data[dataLength+3] & 0xFC); // reset read and ack bits
			uint8_t *ptr = osMemoryPoolAlloc(memPool,osWaitForever);
			memcpy(ptr,data,dataLength+4);
			macSmessage.anyPtr = ptr;
			osMessageQueuePut(queue_phyS_id,&macSmessage,osPriorityNormal,osWaitForever);// send back the message
		}
		else{
				if(nResend == 2){
					sendMacError();
				}
				nResend = 0;// reset the counter
				sendData();
			}				
		}			
			break;
	}
		osMemoryPoolFree(memPool,data);// release the memory
}
}


