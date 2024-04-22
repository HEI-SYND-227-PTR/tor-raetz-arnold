#include "main.h"
#include "stm32f7xx_hal.h"

uint8_t calcCheckSum(uint8_t* data, uint8_t length){
	uint8_t result = 0;
	for(int i = 0;i < result;i++){
		result += data[i];
	}
	return result;
}

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
		else
		{
				uint8_t addressMask = 0x78;
				uint8_t sapiMask = 0x7;
				uint8_t checksumMask = 0xFC;
			  uint8_t dataLength = data[2];
			
				if((data[0] & addressMask) == gTokenInterface.myAddress <<3)// if the source address  is my address
				{
					// send DATABACK message
					message.type = DATABACK;
					osMessageQueuePut(queue_macS_id,&message,osPriorityNormal,osWaitForever);
				}
				else
				{
					if((calcCheckSum(data,dataLength)<<2) == (data[dataLength+3]&checksumMask)){// if checksum is correct
						
						if((data[1] & addressMask) == gTokenInterface.myAddress <<3 || (data[1] & addressMask) == 0xF <<3)// if the destination address is my address or broadcast
						{	
							uint8_t destSapi = data[1] & sapiMask;
							if((destSapi == CHAT_SAPI && gTokenInterface.connected)|| destSapi == TIME_SAPI){ // if the targeted sapi match with one of our active sapis
								uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
								struct queueMsg_t newMSG;
								newMSG.type = DATA_IND;
								newMSG.addr = (data[0] & addressMask)>>3;
								newMSG.sapi = data[0] & sapiMask;
								newMSG.anyPtr = ptr;
								memcpy(ptr,data,dataLength);
								
								osMessageQueuePut((destSapi==CHAT_SAPI)?queue_chatR_id:queue_timeR_id,&newMSG,osPriorityNormal,osWaitForever);// send the message to the correct message queue
							}				
						}
						data[dataLength+3] |= 0x3; // Put read and Ack bits to 1
					}
					else{
						data[dataLength+3] |= 0x2; // Put read  bit to 1 and ack bit to 0
					}
					
					osMessageQueuePut(queue_phyS_id,&message,NULL,osWaitForever); // send back message to physic layer
				}
			}
	}
}


