
#ifndef __USB_H
#define __USB_H

/* Includes ------------------------------------------------------------------*/
#include <stm32f10x.h>

#define SW_POWER_PORT											GPIOB
#define SW_POWER_PIN											GPIO_Pin_8
#define USB_DISCONNECT_PORT               GPIOC  
#define USB_DISCONNECT_PIN                GPIO_Pin_13

#define STANDARD_REQUEST 	0
#define CLASS_REQUEST 		0x20
#define VENDOR_REQUEST 		0x40

#define GET_MAX_LUN      0xFE

#ifndef NULL
#define NULL    ((void *)0)
#endif

typedef enum{
	ATTACHED=0,
	POWERED,
	DEFAULT,
	ADDRESS,
	CONFIGURED,
	SUSPENDED
}USB_STATE;

typedef enum{
	Buffer_READY=0,
	Buffer_EMPTY,
	Buffer_FAIL
}SDBufferState;

typedef enum{
	SD_READY=0,
	SD_BUSY,
	SD_ERR
}SDTranState;


typedef enum{
	IDLESTAGE=0,
	SETUPSTAGE,
	DATASTAGE,
	STATUSSTAGE
}STAGE;



typedef enum _STANDARD_REQUESTS
{
  GET_STATUS = 0,
  CLEAR_FEATURE,
  RESERVED1,
  SET_FEATURE,
  RESERVED2,
  SET_ADDRESS,
  GET_DESCRIPTOR,
  SET_DESCRIPTOR,
  GET_CONFIGURATION,
  SET_CONFIGURATION,
  GET_INTERFACE,
  SET_INTERFACE,
  /*TOTAL_sREQUEST, */ /* Total number of Standard request */
  SYNCH_FRAME = 12
} STANDARD_REQUESTS;

/* bmRequestType.Recipient */
#define REQUEST_TO_DEVICE          0
#define REQUEST_TO_INTERFACE       1
#define REQUEST_TO_ENDPOINT        2
#define REQUEST_TO_OTHER           3


#define  	USB_ADDR0_TX 		*((uint32_t*)(PMAAddr+0))
#define 	USB_COUNT0_TX		*((uint32_t*)(PMAAddr+4))
#define		USB_ADDR0_RX		*((uint32_t*)(PMAAddr+8))
#define		USB_COUNT0_RX		*((uint32_t*)(PMAAddr+0xC))

typedef struct{
	uint32_t	USB_ADDRx_TX;
	uint32_t 	USB_COUNTx_TX;
	uint32_t	USB_ADDRx_RX;
	uint32_t 	USB_COUNTx_RX;
}EPx;



void EP_Status (uint8_t EPNum, uint16_t stat); 


uint16_t	Read_EPbuffer(uint8_t ENDPx,uint8_t *pData);
void Write_EPbuffer(uint8_t ENDPx,uint8_t *pData,uint16_t cnt);
	
void EPx_Reset(void);
void SETUP0Process(void);
void IN0Process(void);
void OUT0Process(void);

void IN1_Process(void);
void OUT2_Process(void);






#endif

