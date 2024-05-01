#include "main.h"
#include "stm32f7xx_hal.h"
#include <string.h> 
		
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
	uint8_t *rxData;
	
		while(true){

		osMessageQueueGet(queue_macR_id,&macRmessage,(uint8_t*)osPriorityNormal,osWaitForever); // get message
		rxData = macRmessage.anyPtr;// get data in the message
	
		if(rxData[0] == 0xFF){// if we received a token frame
			uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
			struct queueMsg_t newMSG;
			newMSG.type = TOKEN;
			newMSG.anyPtr = ptr;
			memcpy(ptr,rxData,17);// copy the data			
			osMessageQueuePut(queue_macS_id,&newMSG,osPriorityNormal,osWaitForever);// we transmit the TOKEN message to the Mac sender's queue
		}
		else
		{
			// get the message's informations
			uint8_t dataLength = rxData[2];
			uint8_t receivedChecksum = (rxData[dataLength+3]&CHECKSUM_MASK);
			uint8_t calculatedChecksum = (calcCheckSum(rxData,dataLength+3)<<2);
			uint8_t sourceAddress = ((rxData[0] & ADDRESS_MASK)>>3);
			uint8_t destAddress = ((rxData[1] & ADDRESS_MASK)>>3);
			uint8_t sourcesapi = (rxData[0] & SAPI_MASK);
			uint8_t destSapi = (rxData[1] & SAPI_MASK);
			
			// send to TIME or chat
			if((destAddress == gTokenInterface.myAddress) || (destAddress == 0xF)){
				if(calculatedChecksum == receivedChecksum){// check if CRC is correct
					if(((destSapi == CHAT_SAPI) && (gTokenInterface.connected))||(destSapi == TIME_SAPI)){ // if the targeted sapi match with one of our active sapis
							uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
							struct queueMsg_t newMSG;
							newMSG.type = DATA_IND;
							newMSG.addr = sourceAddress;// indicate the source's address
							newMSG.sapi = sourcesapi;// indicate the source's SAPI
							newMSG.anyPtr = ptr;
							memcpy(ptr,&rxData[3],dataLength);// copy the data
							ptr[dataLength] = 0;
							//osMessageQueuePut((destSapi==CHAT_SAPI)?queue_chatR_id:queue_timeR_id,&newMSG,osPriorityNormal,osWaitForever);// send the message to the correct message queue
							if(osMessageQueuePut((destSapi==CHAT_SAPI)?queue_chatR_id:queue_timeR_id,&newMSG,osPriorityNormal,0)!= osOK){// send the message to the correct message queue
								osMemoryPoolFree(memPool,ptr);//release memory if we can't put theb message in the queue
							}
						}
				}
			}
			
			// databack
			if(sourceAddress == gTokenInterface.myAddress){
					uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
					struct queueMsg_t newMSG;
					newMSG.type = DATABACK;
					newMSG.anyPtr = ptr;
					memcpy(ptr,rxData,dataLength+4);// copy the data
				
					// update read and ack bits if I was the sender
					if((calculatedChecksum == receivedChecksum) && (destAddress == gTokenInterface.myAddress)){
							ptr[dataLength+3] |= 0x3; // Put read and Ack bits to 1
						}
						else{
							ptr[dataLength+3] |= 0x2; // Put read bit to 1 and Ack bit to 0
					}

					osMessageQueuePut(queue_macS_id,&newMSG,osPriorityNormal,osWaitForever);//send to macS
				
			}		
			// to physical layer
			else{

					uint8_t* ptr = osMemoryPoolAlloc(memPool,osWaitForever);// allocate memory		
					struct queueMsg_t newMSG;
					newMSG.type = TO_PHY;
					newMSG.anyPtr = ptr;
					memcpy(ptr,rxData,dataLength+4);// copy the data
					if(destAddress != 0xF){// if it is not a broadcast message
						if(calculatedChecksum == receivedChecksum){
							ptr[dataLength+3] |= 0x3; // Put read and Ack bits to 1
						}
						else{
							ptr[dataLength+3] |= 0x2; // Put read bit to 1 and Ack bit to 0
						}
					}
					
					osMessageQueuePut(queue_phyS_id,&newMSG,osPriorityNormal,osWaitForever); // send back message to physic layer
			}

			
		}
		osMemoryPoolFree(memPool,rxData);// release the memory
	}
}



