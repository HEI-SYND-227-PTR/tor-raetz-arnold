#include "main.h"
#include "stm32f7xx_hal.h"

		
uint8_t calcCheckSum(uint8_t* data, uint8_t length){
	uint8_t result = 0;
	for(int i = 0;i < length;i++){
		result += data[i];
	}
	return result;
}

void MacReceiver(void *argument)
{
	
	struct queueMsg_t macRmessage;
	uint8_t *data;
	
	while(true){

		osMessageQueueGet(queue_macR_id,&macRmessage,NULL,osWaitForever); // get message
		data = macRmessage.anyPtr;// get data in the message
	
		if(data[0] == 0xFF){// if we received a token frame
			macRmessage.type = TOKEN;// change message type to token
			osMessageQueuePut(queue_macS_id,&macRmessage,osPriorityNormal,osWaitForever);// we transmit the TOKEN message to the Mac sender's queue
		}
		else
		{
			  uint8_t dataLength = data[2]; // get the length of the message
			
				if((data[0] & ADDRESS_MASK) == gTokenInterface.myAddress <<3)// if the source address  is my address
				{
					// send DATABACK message
					macRmessage.type = DATABACK;
					osMessageQueuePut(queue_macS_id,&macRmessage,osPriorityNormal,osWaitForever);
					
					if((data[1] & ADDRESS_MASK) == (0xF <<3)){// Also, if this is a broadcast message, we need to transmit it to the time receiver
								
						uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
								struct queueMsg_t newMSG;
								newMSG.type = DATA_IND;
								newMSG.addr = (data[0] & ADDRESS_MASK)>>3;// indicate the source's address
								newMSG.sapi = data[0] & SAPI_MASK;
								newMSG.anyPtr = ptr;
								memcpy(ptr,&data[3],dataLength);// make a copy of the data 
								
								osMessageQueuePut(queue_timeR_id,&newMSG,osPriorityNormal,osWaitForever);// send the message to time receiver queue
					}		
				}
				else // if the source address is not my address
				{
					uint8_t receivedChecksum = (data[dataLength+3]&CHECKSUM_MASK);// get the checksum in the message
					uint8_t calculatedChecksum = (calcCheckSum(data,dataLength+3)<<2);// recalculate the checksum
					if(calculatedChecksum == receivedChecksum){// if checksum is correct
						
						if(((data[1] & ADDRESS_MASK) == (gTokenInterface.myAddress <<3)) || ((data[1] & ADDRESS_MASK) == (0xF <<3)))// if the destination address is my address or broadcast
						{	
							uint8_t destSapi = data[1] & SAPI_MASK;// get the destination SAPI
							if((destSapi == CHAT_SAPI && gTokenInterface.connected)|| (destSapi == TIME_SAPI)){ // if the targeted sapi match with one of our active sapis
								
								// create a DATA_IND message and copy the data from macRessage
								uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
								struct queueMsg_t newMSG;
								newMSG.type = DATA_IND;
								newMSG.addr = (data[0] & ADDRESS_MASK)>>3;// indicate the source's address
								newMSG.sapi = data[0] & SAPI_MASK;// indicate the source's SAPI
								newMSG.anyPtr = ptr;
								memcpy(ptr,&data[3],dataLength);// copy the data
								
								osMessageQueuePut((destSapi==CHAT_SAPI)?queue_chatR_id:queue_timeR_id,&newMSG,osPriorityNormal,osWaitForever);// send the message to the correct message queue
							}				
						}
						data[dataLength+3] |= 0x3; // Put read and Ack bits to 1
					}
					else{// if the checksum is not correct
						data[dataLength+3] |= 0x2; // Put read  bit to 1 and ack bit to 0
					}
					
					osMessageQueuePut(queue_phyS_id,&macRmessage,NULL,osWaitForever); // send back message to physic layer
				}
			}
	}
}



