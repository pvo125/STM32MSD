#include <stm32f10x.h>

#include "usb_regs.h"
#include "usb_desc.h"
#include "usb.h"
#include "bot.h"
#include "sdcard.h"
#include "my_sdcard.h"

uint8_t BOTState=BOTIDLE;

CBW cbw;
CSW csw={0x53425355,0,0,0};

#define COUNTSECTORS_TO_BUFFER		16
uint8_t sdRXBuffer[COUNTSECTORS_TO_BUFFER*SDSECTOR_SIZE];
uint8_t sdTXBuffer[2*SDSECTOR_SIZE];

uint8_t *psdTXBuffer,*psdRXBuffer;

uint32_t LBAAddr;
uint16_t countSect;
uint16_t countbyte;
uint16_t countbyte_rx;

extern uint32_t sdBlockCNT;
extern uint32_t sdBlockLEN;

extern volatile SDTranState 	sdTranState;
extern SD_Error sd_error;

extern uint8_t Mode_Sense6_data[];
extern uint8_t Page00_Inquiry_Data[]; 
extern  uint8_t Scsi_Sense_Data[];
extern uint8_t Standard_Inquiry_Data[];
extern uint8_t ReadCapacity10_Data[];
extern uint8_t ReadFormatCapacity_Data[];
void CBWDecode(void);

extern SD_CardInfo sdcardinfo;
/*
*/
void IN1_Process(void){
	switch(BOTState){
		case SEND_CSW:			// Отправляем статусный блок во время транзакции IN
			Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
			BOTState=BOTIDLE;													// Готов принять следующий командный блок CBW
		break;	
		case BOTDATA_IN:
			if(csw.dCSWDataRecidue)
			{
				if(countbyte==SDSECTOR_SIZE)
				 {
			 	   LBAAddr+=SDSECTOR_SIZE;
				   StartReadBlockDMA(LBAAddr,sdTXBuffer);
					 psdTXBuffer=(uint8_t*)(sdTXBuffer+SDSECTOR_SIZE);
				 }
				else if(countbyte==(SDSECTOR_SIZE*2))
				{
					 LBAAddr+=SDSECTOR_SIZE;
					 StartReadBlockDMA(LBAAddr,(sdTXBuffer+SDSECTOR_SIZE));
					 countbyte=0;
					 psdTXBuffer=(uint8_t*)sdTXBuffer;
				}					
				Write_EPbuffer(EP1_IN, psdTXBuffer,64);
				countbyte+=64;
				csw.dCSWDataRecidue-=64;
				psdTXBuffer+=64; 
			}
			else
			{
				csw.bCSWStatus=COMMANDPASSED;
				Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
				BOTState=BOTIDLE;	
			}				
		break;
		case BOTLAST_DATA_IN:
									
		break;	
	}
}
/*
*/
void OUT2_Process(void){
	
	switch(BOTState){
		case BOTIDLE:
			CBWDecode();
		break;	
		case BOTDATA_OUT:
				Read_EPbuffer(EP2_OUT,psdRXBuffer);
				countbyte_rx+=64;
				psdRXBuffer+=64;
				csw.dCSWDataRecidue-=64;
				if(countbyte_rx==COUNTSECTORS_TO_BUFFER*SDSECTOR_SIZE)
			 {
				sdTranState=SD_BUSY;
				StartWriteMultiBlocksDMA(LBAAddr,sdRXBuffer,COUNTSECTORS_TO_BUFFER);
				LBAAddr+=countbyte_rx;
				psdRXBuffer=sdRXBuffer;
				countbyte_rx=0;
				while(sdTranState==SD_BUSY)  {}
				SDCmdSetSendStatus(sdcardinfo.RCA);	
				while((SDIO->RESP1 & READY_FOR_DATA)==0)
					SDCmdSetSendStatus(sdcardinfo.RCA);
				if(csw.dCSWDataRecidue==0)
				{
					csw.bCSWStatus=COMMANDPASSED;
					Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
					BOTState=BOTIDLE;	
				}
			 }
			 if((csw.dCSWDataRecidue==0)&&(countbyte_rx))
			 {
				sdTranState=SD_BUSY;
				if(countbyte_rx==0x200)
				{
					StartWriteBlockDMA(LBAAddr,sdRXBuffer);
					while(sdTranState==SD_BUSY)  {}
					SDCmdSetSendStatus(sdcardinfo.RCA);	
					while((SDIO->RESP1 & READY_FOR_DATA)==0)
						SDCmdSetSendStatus(sdcardinfo.RCA);
						
				}
				else
				{
					StartWriteMultiBlocksDMA(LBAAddr,sdRXBuffer,countbyte_rx/512);
					while(sdTranState==SD_BUSY)  {}	
					SDCmdSetSendStatus(sdcardinfo.RCA);	
					while((SDIO->RESP1 & READY_FOR_DATA)==0)
						SDCmdSetSendStatus(sdcardinfo.RCA);
				 }
				 countbyte_rx=0;
				 psdRXBuffer=sdRXBuffer;
				 // Переключаем EP1_IN DTOG_TX в 1
				 //	if((*EPxR(1) & EP_DTOG_TX)==0) _ToggleDTOG_TX(ENDP1);
			   csw.bCSWStatus=COMMANDPASSED;
				 Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
				 BOTState=BOTIDLE;	
			 }
		break;
	}
}
/*
*/
void CBWDecode(void){
		
		Read_EPbuffer(EP2_OUT,(uint8_t*)&cbw);
		csw.dCSWTag=cbw.dCBWTag;
		csw.dCSWDataRecidue=cbw.dCBWDataTransferLength;
		switch(cbw.CBWCB[0]){
			case SCSI_TESTUNITREADY:
				if(sd_error==SD_OK)
				{csw.bCSWStatus=COMMANDPASSED;
					Scsi_Sense_Data[2]=NO_SENSE;
					Scsi_Sense_Data[12]=NO_SENSE;
				}
				else
				{	csw.bCSWStatus=COMMANDFAILED;
					Scsi_Sense_Data[2]=NOT_READY;
					Scsi_Sense_Data[12]=MEDIUM_NOT_PRESENT;
				}
				csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
				Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
				BOTState=BOTIDLE;														
			break;	
			case SCSI_REQUESTSENSE:
				if(cbw.CBWCB[4]>18)
					cbw.CBWCB[4]=18;
				// Записываем массив INQUIRY в буфер EP1_IN на отправку и отправляем его выставив EP1_IN stat_tx  в VALID
				Write_EPbuffer(EP1_IN, Scsi_Sense_Data,cbw.CBWCB[4]);
				BOTState=SEND_CSW;
				csw.bCSWStatus=COMMANDPASSED;
				csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
			break;
			case SCSI_INQUIRY:
				if(cbw.CBWCB[1]==0)
				{	// Записываем массив INQUIRY в буфер EP1_IN на отправку и отправляем его выставив EP1_IN stat_tx  в VALID
					Write_EPbuffer(EP1_IN, Standard_Inquiry_Data,36);
					/* Заполняем структуру для передачи статуса которая сосоится при след. IN транзакции и состоянии BOTState=SEND_CSW */
					BOTState=SEND_CSW;
					csw.bCSWStatus=COMMANDPASSED;
					csw.dCSWDataRecidue-=/*cbw.dCBWDataTransferLength;*/36;
				}
				else
				{	/*csw.bCSWStatus=COMMANDFAILED;
					csw.dCSWDataRecidue=cbw.dCBWDataTransferLength;
					Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
				// Заполняем структуру для передачи статуса которая сосоится при след. IN транзакции и состоянии BOTState=SEND_CSW 
					BOTState=SEND_CSW;
					csw.bCSWStatus=COMMANDPASSED;
					csw.dCSWDataRecidue=cbw.dCBWDataTransferLength;*/
					//if(*EPxR(1) & EP_DTOG_TX)	_ToggleDTOG_TX(ENDP1);
					Write_EPbuffer(EP1_IN, Page00_Inquiry_Data,5);
					// Заполняем структуру для передачи статуса которая сосоится при след. IN транзакции и состоянии BOTState=SEND_CSW 
					BOTState=SEND_CSW;
					csw.bCSWStatus=COMMANDPASSED;
					csw.dCSWDataRecidue-=/*cbw.dCBWDataTransferLength;*/5;
				}
			break;
			case SCSI_ALLOW_MEDIUM_REMOVAL:
				csw.bCSWStatus=COMMANDPASSED;
        csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
				Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
				BOTState=BOTIDLE;		
			break;		
			case SCSI_READFORMATCAPACITY:
				if(sd_error==SD_OK)
				{	ReadFormatCapacity_Data[4]=(uint8_t)(sdBlockCNT>>24);
					ReadFormatCapacity_Data[5]=(uint8_t)(sdBlockCNT>>16);
					ReadFormatCapacity_Data[6]=(uint8_t)(sdBlockCNT>>8);
					ReadFormatCapacity_Data[7]=(uint8_t)sdBlockCNT;
					ReadFormatCapacity_Data[9]=(uint8_t)(sdBlockLEN>>16);
					ReadFormatCapacity_Data[10]=(uint8_t)(sdBlockLEN>>8);
					ReadFormatCapacity_Data[11]=(uint8_t)sdBlockLEN;
					// Переключаем EP1_IN DTOG_TX в 1
					//if((*EPxR(1) & EP_DTOG_TX)==0) _ToggleDTOG_TX(ENDP1);
					// Записываем массив ReadFormatCapacity_Data в буфер EP1_IN на отправку и отправляем его выставив EP1_IN stat_tx  в VALID
					Write_EPbuffer(EP1_IN, ReadFormatCapacity_Data,12);
				// Заполняем структуру для передачи статуса которая сосоится при след. IN транзакции и состоянии BOTState=SEND_CSW 
					BOTState=SEND_CSW;
					csw.bCSWStatus=COMMANDPASSED;
					csw.dCSWDataRecidue-=/*cbw.dCBWDataTransferLength;*/12;
				}
				else
				{// Заполняем структуру для передачи статуса ошибки и отправляем его 
					csw.bCSWStatus=COMMANDFAILED;
					csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
					BOTState=BOTIDLE;
					Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
				}
			break;
			case SCSI_READCAPACITY10:
				if(sd_error==SD_OK)
				{	 ReadCapacity10_Data[0]=(uint8_t)((sdBlockCNT-1)>>24);
					 ReadCapacity10_Data[1]=(uint8_t)((sdBlockCNT-1)>>16);
					 ReadCapacity10_Data[2]=(uint8_t)((sdBlockCNT-1)>>8);
					 ReadCapacity10_Data[3]=(uint8_t)((sdBlockCNT-1));
					 ReadCapacity10_Data[4]=(uint8_t)(sdBlockLEN>>24);
					 ReadCapacity10_Data[5]=(uint8_t)(sdBlockLEN>>16);
					 ReadCapacity10_Data[6]=(uint8_t)(sdBlockLEN>>8);
					 ReadCapacity10_Data[7]=(uint8_t)sdBlockLEN;
					 Write_EPbuffer(EP1_IN, ReadCapacity10_Data,8);
				/* Заполняем структуру для передачи статуса которая сосоится при след. IN транзакции и состоянии BOTState=SEND_CSW */
					 BOTState=SEND_CSW;
					 csw.bCSWStatus=COMMANDPASSED;
					 csw.dCSWDataRecidue-=/*cbw.dCBWDataTransferLength;*/8;
				}
				else
				{	// Заполняем структуру для передачи статуса ошибки и отправляем его 
					csw.bCSWStatus=COMMANDFAILED;
					csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
					BOTState=BOTIDLE;
					Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
				}
			break;
			case SCSI_READ10:
				LBAAddr=(cbw.CBWCB[2]<<24)|(cbw.CBWCB[3]<<16)|(cbw.CBWCB[4]<<8)|cbw.CBWCB[5];
				countSect=(cbw.CBWCB[7]<<8)|cbw.CBWCB[8];
				LBAAddr*=SDSECTOR_SIZE;
				if(countSect>1)
				{
					sdTranState=SD_BUSY;
					if(StartReadBlockDMA(LBAAddr,sdTXBuffer)!=SD_OK)
					{// Заполняем структуру для передачи статуса ошибки и отправляем его 
						csw.bCSWStatus=COMMANDFAILED;
						csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
						BOTState=BOTIDLE;
						Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
						break;
					}
					LBAAddr+=SDSECTOR_SIZE;
					while(sdTranState==SD_BUSY){}	
					if(StartReadBlockDMA(LBAAddr,(sdTXBuffer+SDSECTOR_SIZE))!=SD_OK)
						{// Заполняем структуру для передачи статуса ошибки и отправляем его 
							csw.bCSWStatus=COMMANDFAILED;
							csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
							BOTState=BOTIDLE;
							Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
							break;
						}
				}
				else
				{
				 if(StartReadBlockDMA(LBAAddr,sdTXBuffer)!=SD_OK)
					{// Заполняем структуру для передачи статуса ошибки и отправляем его 
							csw.bCSWStatus=COMMANDFAILED;
							csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
							BOTState=BOTIDLE;
							Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
							break;
					}	
				 while(!(DMA2->ISR & DMA_ISR_HTIF4)){}	
				}
				psdTXBuffer=sdTXBuffer;
				Write_EPbuffer(EP1_IN, psdTXBuffer,64);
				psdTXBuffer+=64;
				countbyte=64;
				csw.dCSWDataRecidue-=64;
				BOTState=BOTDATA_IN;		
			break;
			case SCSI_MODESENSE6:
				// Записываем массив Mode_Sense6_data в буфер EP1_IN на отправку и отправляем его выставив EP1_IN stat_tx  в VALID
				Write_EPbuffer(EP1_IN, Mode_Sense6_data,4);
				/* Заполняем структуру для передачи статуса которая сосоится при след. IN транзакции и состоянии BOTState=SEND_CSW */
				BOTState=SEND_CSW;
				csw.bCSWStatus=COMMANDPASSED;
				csw.dCSWDataRecidue-=/*cbw.dCBWDataTransferLength; */4;
 			break;		
			case SCSI_START_STOP_UNIT:
				csw.bCSWStatus=COMMANDPASSED;
        csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
				Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
				BOTState=BOTIDLE;		
      break;
			case SCSI_WRITE10:
				LBAAddr=(cbw.CBWCB[2]<<24)|(cbw.CBWCB[3]<<16)|(cbw.CBWCB[4]<<8)|cbw.CBWCB[5];
				countSect=(cbw.CBWCB[7]<<8)|cbw.CBWCB[8];
				LBAAddr*=SDSECTOR_SIZE;
				psdRXBuffer=sdRXBuffer;
				countbyte_rx=0;
				BOTState=BOTDATA_OUT;
			break;
			case SCSI_VERIFY10:
				if((cbw.CBWCB[1] & 0x02)==0)
				{
					Scsi_Sense_Data[2]=NO_SENSE;
					Scsi_Sense_Data[12]=NO_SENSE;
					csw.bCSWStatus=COMMANDPASSED;
					csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
					Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
					BOTState=BOTIDLE;
				}					
			break;	
			default:
				// Заполним массив Scsi_Sense_Data для передачи подробных данных при запросе SCSI_REQUESTSENSE
				Scsi_Sense_Data[2]=ILLEGAL_REQUEST;
				Scsi_Sense_Data[12]=INVALID_COMMAND;
				EP_Status(EP1_IN,EP_TX_STALL);	// На неизвестную команду ответим STALL 
				/*csw.bCSWStatus=COMMANDFAILED;
				csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
				Write_EPbuffer(EP1_IN, (uint8_t*)&csw,13);
				// Заполняем структуру для передачи статуса которая сосоится при след.IN транзакции и состоянии BOTState=SEND_CSW 
				csw.bCSWStatus=COMMANDPASSED;
				csw.dCSWDataRecidue-=cbw.dCBWDataTransferLength;
				BOTState=SEND_CSW;*/
			break;
	}
}

/*
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
