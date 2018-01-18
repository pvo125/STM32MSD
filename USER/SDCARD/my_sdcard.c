
#include <stm32f10x.h>
#include "sdcard.h"
#include "my_sdcard.h"
#include "usb.h"

extern SD_CardInfo sdcardinfo;
extern volatile uint32_t	StopCondition;

volatile SDBufferState sdbufferState=Buffer_EMPTY;
volatile SDTranState 	 sdTranState=SD_READY;
volatile SDTranState   sdReadyforData=SD_READY;

SD_Error SDCmdSetSendStatus(uint32_t rca){
	SD_Error errorstatus = SD_OK;
// CPSM CMD13	SDIO_SEND_STATUS
		
			SDIO->CMD=0;
			SDIO->ARG=rca<<16;
			SDIO->CMD |=SDIO_SEND_STATUS						// CMD13 SDIO_SEND_STATUS
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD16
			while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL)))	{}	// Ждем response от карты	
			if(SDIO->STA & SDIO_STA_CMDREND)
				SDIO->ICR=SDIO_ICR_CMDRENDC;
			else
			{	
				errorstatus=SD_CMD_CRC_FAIL;	
				SDIO->ICR =SDIO_ICR_CCRCFAILC;
				return errorstatus;
			}
	return errorstatus;		
}

/*
*/
SD_Error SDCmdSetBlockLen(uint16_t len){
	SD_Error errorstatus = SD_OK;
	// CPSM CMD16	SET_BLOCKLEN
		SDIO->CMD=0;
		SDIO->ARG=len;
		SDIO->CMD |=SDIO_SET_BLOCKLEN						// CMD16 SET_BLOCKLEN
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD16
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL)))	{}	// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else
		{	
			errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}		
	return errorstatus;
}	
/*
*/
SD_Error StartReadBlockDMA(uint32_t addr,uint8_t * buffer){
		SD_Error errorstatus = SD_OK;

// CPSM CMD17	READ_SINGLE_BLOCK
		
		SDIO->CMD=0;
		SDIO->ARG=addr;
		SDIO->CMD |=SDIO_READ_SINGLE_BLOCK			// CMD17 READ_SINGLE_BLOCK
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD17
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL|SDIO_STA_CTIMEOUT)))	{}	// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else if(SDIO->STA & SDIO_STA_CTIMEOUT)
		{	SDIO->ICR=SDIO_ICR_CTIMEOUTC;
			errorstatus=SD_CMD_RSP_TIMEOUT;
			return errorstatus;
		}
		else
		{	errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}	
// DPSM
		SDIO->DCTRL=0;
		SDIO->DTIMER=0x00FFFFFFF;
		SDIO->DLEN=512;
		SDIO->DCTRL |=SDIO_DCTRL_DBLOCKSIZE_0|SDIO_DCTRL_DBLOCKSIZE_3		// (1001) 512  Data block size		
								|SDIO_DCTRL_DMAEN																		// DMA enabled.
							/*|SDIO_DCTRL_DTMODE*/																// Block data transfer.
								|SDIO_DCTRL_DTDIR 																	// From card to controller.	
								|SDIO_DCTRL_DTEN;																		// Data transfer enabled 		
		
//DMA2_Channel4
		DMA2->IFCR = DMA_IFCR_CGIF4; 
		DMA2_Channel4->CCR &=~DMA_CCR4_EN;
		DMA2_Channel4->CCR &=~DMA_CCR4_DIR;				// 0: Read from peripheral
		DMA2_Channel4->CPAR=(uint32_t)0x40018080;	//SDIO_FIFO_Address             
		DMA2_Channel4->CMAR=(uint32_t)buffer;
		DMA2_Channel4->CNDTR=128;									// 512/4=128
		DMA2_Channel4->CCR |=DMA_CCR4_PL_1				//Channel Priority level - high
											 |DMA_CCR4_MSIZE_1			// 32 bits		
											 |DMA_CCR4_PSIZE_1			// 32 bits	
											 |DMA_CCR4_MINC					// Memory increment mode enable
										 /*|DMA_CCR4_PINC*/				// Peripheral increment mode disable
										 /*|DMA_CCR4_CIRC*/				// Circular mode disable
										 /*|DMA_CCR4_DIR*/				// 0: Read from peripheral
											 |DMA_CCR4_TCIE				// Transfer complete interrupt enable
											 |DMA_CCR4_EN;						//Channel enable 	
		return errorstatus;	
}				
/*
*/
SD_Error StartReadMultiBlocksDMA(uint32_t addr,uint8_t * buffer,uint32_t count){
	SD_Error errorstatus = SD_OK;
	
// CPSM CMD18	READ_MULTI_BLOCKS
		SDIO->CMD=0;
		SDIO->ARG=addr;
		SDIO->CMD |=SDIO_READ_MULT_BLOCK			// CMD18 READ_MULT_BLOCK
							|SDIO_CMD_WAITRESP_0				// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;						// CPSM IS ENABLE			// Отправляем команду CMD18
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL|SDIO_STA_CTIMEOUT)))	{}	// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else if(SDIO->STA & SDIO_STA_CTIMEOUT)
		{	SDIO->ICR=SDIO_ICR_CTIMEOUTC;
			errorstatus=SD_CMD_RSP_TIMEOUT;
			return errorstatus;
		}
		else
		{	errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}	
// DPSM		
		SDIO->DCTRL=0;
		SDIO->DLEN=512*count;
		SDIO->DTIMER=0x00FFFFFFF;
		SDIO->DCTRL |=SDIO_DCTRL_DBLOCKSIZE_0|SDIO_DCTRL_DBLOCKSIZE_3		// (1001) 512  Data block size		
								|SDIO_DCTRL_DMAEN																		// DMA enabled.
							/*|SDIO_DCTRL_DTMODE*/																// Block data transfer.
								|SDIO_DCTRL_DTDIR 																	// From card to controller.	
								|SDIO_DCTRL_DTEN;																		// Data transfer enabled 	
		
		StopCondition=1;
//DMA2_Channel4
		DMA2->IFCR = DMA_IFCR_CGIF4; 
		DMA2_Channel4->CCR &=~DMA_CCR4_EN;
		DMA2_Channel4->CCR &=~DMA_CCR4_DIR;				// 0: Read from peripheral
		DMA2_Channel4->CPAR=(uint32_t)0x40018080;	//SDIO_FIFO_Address             
		DMA2_Channel4->CMAR=(uint32_t)buffer;
		DMA2_Channel4->CNDTR=128*count;						// 512/4*count
		DMA2_Channel4->CCR |=DMA_CCR4_PL_1				//Channel Priority level - high
											 |DMA_CCR4_MSIZE_1			// 32 bits		
											 |DMA_CCR4_PSIZE_1			// 32 bits	
											 |DMA_CCR4_MINC					// Memory increment mode enable
										 /*|DMA_CCR4_PINC*/				// Peripheral increment mode disable
										 /*|DMA_CCR4_CIRC*/				// Circular mode disable
										 /*|DMA_CCR4_DIR*/				// 0: Read from peripheral
											 |DMA_CCR4_TCIE				// Transfer complete interrupt enable
											 |DMA_CCR4_EN;						//Channel enable 	
		return errorstatus;		
}

/*
*/
SD_Error StartWriteBlockDMA(uint32_t addr,uint8_t *buffer){
		SD_Error errorstatus=SD_OK;
// CPSM CMD24	WRITE_SINGLE_BLOCK
		
		SDIO->CMD=0;
		SDIO->DLEN=512;
		SDIO->ARG=addr;
		SDIO->CMD |=SDIO_WRITE_SINGLE_BLOCK			// CMD24 WRITE_SINGLE_BLOCK
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD17
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL|SDIO_STA_CTIMEOUT)))	{}	// Ждем response от карты		// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else if(SDIO->STA & SDIO_STA_CTIMEOUT)
		{	SDIO->ICR=SDIO_ICR_CTIMEOUTC;
			errorstatus=SD_CMD_RSP_TIMEOUT;
			return errorstatus;
		}
		else
		{	errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}	
		
		SDIO->MASK |=SDIO_STA_DATAEND|SDIO_STA_DCRCFAIL;						// Enable DATAEND DCRCFAIL  interrupt			
// DPSM
		SDIO->DCTRL=0;
		SDIO->DCTRL |=SDIO_DCTRL_DBLOCKSIZE_0|SDIO_DCTRL_DBLOCKSIZE_3		// (1001) 512  Data block size		
								|SDIO_DCTRL_DMAEN																		// DMA enabled.
							/*|SDIO_DCTRL_DTMODE*/																// Block data transfer.
							/*|SDIO_DCTRL_DTDIR*/ 																// From controller to card.	
								|SDIO_DCTRL_DTEN;																		// Data transfer enabled 		

//DMA2_Channel4
		DMA2->IFCR = DMA_IFCR_CGIF4; 
		DMA2_Channel4->CCR &=~DMA_CCR4_EN;
		DMA2_Channel4->CCR &=~ DMA_CCR4_TCIE;			// Transfer complete interrupt disable
		DMA2_Channel4->CPAR=(uint32_t)0x40018080;	//SDIO_FIFO_Address             
		DMA2_Channel4->CMAR=(uint32_t)buffer;
		DMA2_Channel4->CNDTR=128;									// 512/4=128
		DMA2_Channel4->CCR |=DMA_CCR4_PL_1				//Channel Priority level - high
											 |DMA_CCR4_MSIZE_1			// 32 bits		
											 |DMA_CCR4_PSIZE_1			// 32 bits	
											 |DMA_CCR4_MINC					// Memory increment mode enable
										 /*|DMA_CCR4_PINC*/				// Peripheral increment mode disable
										 /*|DMA_CCR4_CIRC*/				// Circular mode disable
											 |DMA_CCR4_DIR				  // 1: Read from memory
											 /*|DMA_CCR4_TCIE	*/		// Transfer complete interrupt enable
											 |DMA_CCR4_EN;					//Channel enable 	

		return errorstatus;			
}

/*
*/
SD_Error StartWriteMultiBlocksDMA(uint32_t addr,uint8_t *buffer,uint32_t count){
	SD_Error errorstatus=SD_OK;
	
		SDIO->CMD=0;
// CPSM CMD55	SDIO_APP_CMD
		SDIO->ARG=sdcardinfo.RCA<<16;
		SDIO->CMD |=SDIO_APP_CMD								// CMD55	SDIO_APP_CMD
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD55
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL|SDIO_STA_CTIMEOUT)))	{}	// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else if(SDIO->STA & SDIO_STA_CTIMEOUT)
		{	SDIO->ICR=SDIO_ICR_CTIMEOUTC;
			errorstatus=SD_CMD_RSP_TIMEOUT;
			return errorstatus;
		}
		else
		{	errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}
// CPSM ACMD23	SDIO_SET_BLOCK_COUNT
		SDIO->CMD=0;	
		SDIO->ARG=count;
		SDIO->CMD |=SDIO_SET_BLOCK_COUNT				// ACMD23	SDIO_SET_BLOCK_COUNT
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD55
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL|SDIO_STA_CTIMEOUT)))	{}	// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else if(SDIO->STA & SDIO_STA_CTIMEOUT)
		{	SDIO->ICR=SDIO_ICR_CTIMEOUTC;
			errorstatus=SD_CMD_RSP_TIMEOUT;
			return errorstatus;
		}
		else
		{	errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}
// CPSM CMD25	WRITE_SINGLE_BLOCK
		SDIO->CMD=0;
		SDIO->DLEN=512*count;
		SDIO->ARG=addr;
		SDIO->CMD |=SDIO_WRITE_MULT_BLOCK				// CMD25 SDIO_WRITE_MULT_BLOCK
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD17
		while(!(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL|SDIO_STA_CTIMEOUT)))	{}	// Ждем response от карты	
		if(SDIO->STA & SDIO_STA_CMDREND)
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		else if(SDIO->STA & SDIO_STA_CTIMEOUT)
		{	SDIO->ICR=SDIO_ICR_CTIMEOUTC;
			errorstatus=SD_CMD_RSP_TIMEOUT;
			return errorstatus;
		}
		else
		{	errorstatus=SD_CMD_CRC_FAIL;	
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
			return errorstatus;
		}
		SDIO->MASK |=SDIO_STA_DATAEND|SDIO_STA_DCRCFAIL;						// Enable DATAEND DCRCFAIL  interrupt			
// DPSM
		SDIO->DCTRL=0;
		SDIO->DCTRL |=SDIO_DCTRL_DBLOCKSIZE_0|SDIO_DCTRL_DBLOCKSIZE_3		// (1001) 512  Data block size		
								|SDIO_DCTRL_DMAEN																		// DMA enabled.
							/*|SDIO_DCTRL_DTMODE*/																// Block data transfer.
							/*|SDIO_DCTRL_DTDIR*/ 																// From controller to card.	
								|SDIO_DCTRL_DTEN;																		// Data transfer enabled 		
		
		StopCondition=1;
//DMA2_Channel4
		DMA2->IFCR = DMA_IFCR_CGIF4; 
		DMA2_Channel4->CCR &=~DMA_CCR4_EN;
		DMA2_Channel4->CCR &=~ DMA_CCR4_TCIE;			// Transfer complete interrupt disable
		DMA2_Channel4->CPAR=(uint32_t)0x40018080;	//SDIO_FIFO_Address             
		DMA2_Channel4->CMAR=(uint32_t)buffer;
		DMA2_Channel4->CNDTR=128*count;						// 512/4*cont
		DMA2_Channel4->CCR |=DMA_CCR4_PL_1				//Channel Priority level - high
											 |DMA_CCR4_MSIZE_1			// 32 bits		
											 |DMA_CCR4_PSIZE_1			// 32 bits	
											 |DMA_CCR4_MINC					// Memory increment mode enable
										 /*|DMA_CCR4_PINC*/				// Peripheral increment mode disable
										 /*|DMA_CCR4_CIRC*/				// Circular mode disable
											 |DMA_CCR4_DIR				  // 1: Read from memory
											 /*|DMA_CCR4_TCIE	*/		// Transfer complete interrupt enable
											 |DMA_CCR4_EN;					//Channel enable 	

		return errorstatus;
}

/*
*/