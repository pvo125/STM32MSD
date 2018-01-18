#include <stm32f10x.h>
#include "usb_regs.h"
#include "usb_desc.h"
#include "usb.h"
#include "bot.h"

extern CSW csw;

extern volatile USB_STATE usbState;

STAGE stage0=IDLESTAGE;
extern uint8_t BOTState;
uint8_t Endpoint_halt=0;

typedef struct{
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
}DESC_DEV;
/*************************/
DESC_DEV EP0Buffer[32];

typedef enum{
	ADDRESS_IDLE=0,
	ADDRESS_SET
} ADDRESS_TypeDef;

ADDRESS_TypeDef _address=ADDRESS_IDLE;
volatile uint16_t temp_address;

typedef  struct _USB_SETUP_PACKET {
  uint8_t	 	bmRequestType;
  uint8_t   bRequest;
  uint16_t 	wValue;
  uint16_t 	wIndex;
  uint16_t 	wLength;
} USB_SETUP_PACKET;

/*
*/

void EP_Status (uint8_t EPNum, uint16_t stat) {
  uint16_t num, val;

  num = EPNum & 0x0F;
  val = *EPxR(num);
  if (EPNum & 0x80) {                      
   	*EPxR(num) = (val ^ (stat & EPTX_STAT)) & (EPREG_MASK | EPTX_STAT);
  } else {                                  
    *EPxR(num) = (val ^ (stat & EPRX_STAT)) & (EPREG_MASK | EPRX_STAT); //(val ^ (stat & EPRX_STAT)) & (EPREG_MASK | EPRX_STAT)
  }
}

/////////////////////////////////////////////////////////////////////////////////////
uint16_t	Read_EPbuffer(uint8_t ENDPx,uint8_t *pData){
	EPx *EPxBuff=(EPx*)PMAAddr;
	uint32_t *p,cnt,i;
	uint8_t ep;
	uint16_t temp;	
	
	ep=ENDPx & 0x0F;
	temp=*EPxR(ep)^0x3000;
	p=(uint32_t*)(PMAAddr+2*(EPxBuff+ep)->USB_ADDRx_RX);	//0x4000 6100 EP0 RX Buffer
	cnt=(EPxBuff+ep)->USB_COUNTx_RX & 0x3FF;
	for(i=0;i<(cnt+1)/2;i++)
	{	*(uint16_t*)pData=(uint16_t)*p++;
			pData+=2;	}
	 
		*EPxR(ep)=(temp & ~0xC070)|0x80;
			
	return cnt;
}
//////////////////////////////////////////////////////////////////////////////////////////
void Write_EPbuffer(uint8_t ENDPx,uint8_t *pData,uint16_t cnt){
	EPx *EPxBuff=(EPx*)PMAAddr;
	uint32_t *p,i;
	uint8_t ep;
	uint16_t temp;
	
	ep=ENDPx & 0x0F;
	temp=*EPxR(ep)^0x30;
	p=(uint32_t*)(PMAAddr+2*(EPxBuff+ep)->USB_ADDRx_TX);
	for(i=0;i<(cnt+1)/2;i++)
	{
		*p++=*(uint16_t*)pData;
		pData+=2;
	}
	(EPxBuff+ep)->USB_COUNTx_TX=cnt;
	
	*EPxR(ep)=(temp & ~0x70C0)|0x8000;
	
}


/**********************************************************************************************/
	
void EPx_Reset(void)
{
	_SetEPType(ENDP0,EP_CONTROL); 	// Тип CONTROL  (ожидаем пакета SETUP)  
			
	*((uint32_t*)(PMAAddr+0))=0x0040;		// USB_ADDR0_TX=0x0040		EP0 TX Buffer =0x40006000+USB_ADDR0_TX*2=0x40006080
	*((uint32_t*)(PMAAddr+4))=0x0;				// USB_COUNT0_TX=0x0
	*((uint32_t*)(PMAAddr+8))=0x0080;		// USB_ADDR0_RX=0x0080		EP0 RX Buffer =0x40006000+USB_ADDR0_RX*2=0x40006100
	*((uint32_t*)(PMAAddr+0xC))=0x8400;		// USB_COUNT0_RX=0x8400	BL_SIZE=1 NUM_BLOCK=1 => memory alocation = 64 bytes
			
	/*Настроим ENDP1 ENDP2 */
	//ENDP1
	*((uint32_t*)(PMAAddr+0x10))=0x00C0;	// USB_ADDR1_TX=0x00C0		EP1 TX Buffer =0x40006000+USB_ADDR1_TX*2=0x40006180
	*((uint32_t*)(PMAAddr+0x14))=0x0;				// USB_COUNT1_TX=0x0
	_SetEPType(ENDP1,EP_BULK); 							// Тип BULK 
	//ENDP2
	*((uint32_t*)(PMAAddr+0x28))=0x0100;			// USB_ADDR2_RX			EP2 RX Buffer =0x40006000+USB_ADDR2_RX*2=0x40006200
	*((uint32_t*)(PMAAddr+0x2C))=0x8400;		// USB_COUNT0_RX=0x8400	BL_SIZE=1 NUM_BLOCK=1 => memory alocation = 64 bytes	
	_SetEPType(ENDP2,EP_BULK); 							// Тип BULK 
	
	/*Разрешаем работу USB функции пока с адресом 0 */
	*DADDR=0x80;		// Enable function + ADDR[6:0]=0
		
	*EPxR(0)^=(EP_RX_VALID|EP_TX_NAK);
	*EPxR(1)^=(EP_CTR_RX|EP_RX_DIS|EP_CTR_TX|EP_TX_VALID);
	*EPxR(2)^=(EP_CTR_RX|EP_RX_VALID|EP_CTR_TX|EP_TX_DIS);
}


/*********************************************************************************************
*
*										void Get_Descriptor
*
**********************************************************************************************/
void Get_Descriptor(uint16_t wValue,uint16_t len){
	
	switch((wValue & 0xFF00)>>8){	// старший байт wValue - тип дескриптора
		case 01:		//////////////////////////////////////////////// device descriptor
			Write_EPbuffer(EP0_IN,(uint8_t*)DeviceDescriptor,len);
			stage0=STATUSSTAGE;
		break;	
		case 02:		//////////////////////////////////////////////// configuration descriptor
			if(len>sizeof(ConfigDescriptor)) 
				len=sizeof(ConfigDescriptor);
			Write_EPbuffer(EP0_IN,(uint8_t*)ConfigDescriptor,len);
		break;
		case 03:		//////////////////////////////////////////////// string descriptor	
			switch(wValue & 0xFF){	// // младший  байт wValue - индекс строки строкового дескриптора
				case 0:		//LANGID  string descriptor
					if(len>MASS_SIZ_STRING_LANGID) 
						len=MASS_SIZ_STRING_LANGID;
					Write_EPbuffer(EP0_IN,(uint8_t*)StringLangID,len);
				break;
				case 1:		//MMANUFACTURER string descriptor
					if(len>MASS_SIZ_STRING_VENDOR) 
						len=MASS_SIZ_STRING_VENDOR;
					Write_EPbuffer(EP0_IN,(uint8_t*)StringVendor,len);
				break;
				case 2:		//PRODUCT string descriptor
					if(len>MASS_SIZ_STRING_PRODUCT) 
						len=MASS_SIZ_STRING_PRODUCT;
					Write_EPbuffer(EP0_IN,(uint8_t*)StringProduct,len);
				break;
				case 3:		//SERIAL_NUMBER string descriptor
					if(len>MASS_SIZ_STRING_SERIAL) 
						len=MASS_SIZ_STRING_SERIAL;
					Write_EPbuffer(EP0_IN,(uint8_t*)StringSerial,len);
				break;
				case 4:		//INTERFACE string descriptor
					if(len>MASS_SIZ_STRING_INTERFACE) 
						len=MASS_SIZ_STRING_INTERFACE;
					Write_EPbuffer(EP0_IN,(uint8_t*)StringInterface,len);
				break;
				default:		//STALL 
					EP_Status(EP0_IN,EP_TX_STALL);
				break;
			}
		break;
		
		default:		//STALL
			EP_Status(EP0_IN,EP_TX_STALL);
		break;		
	}
}
/*

*/
void SETUP0Process(void)
{
	uint8_t lun;
	USB_SETUP_PACKET SetupPacket;
	static DESC_DEV *p=EP0Buffer;
	uint8_t buffer[2];
	
	Read_EPbuffer(EP0_OUT,(uint8_t*)&SetupPacket);	
	//*EPxR(0) = (*EPxR(0) ^ EP_RX_VALID) & (EPREG_MASK | EPRX_STAT);
		p->bmRequestType=SetupPacket.bmRequestType;
		p->bRequest=SetupPacket.bRequest;
		p->wValue=SetupPacket.wValue;
		p->wIndex=SetupPacket.wIndex;
		p->wLength=SetupPacket.wLength;
		p++;
		switch(SetupPacket.bmRequestType & 0x60){	// выделяем  5 6 биты 0-standard 1-class 2-vendor
			case STANDARD_REQUEST:
				switch(SetupPacket.bmRequestType & 0x3){ // получатель 0-устройство 1-интерфейс 2- конечная точка
							case REQUEST_TO_DEVICE:
								switch(SetupPacket.bRequest){
										case GET_STATUS:
											buffer[0]=0x03;	//(  Remote Wakeup and Self-powered )
											buffer[1]=0x00;
											Write_EPbuffer(EP0_IN,buffer,2);
										break;
										case CLEAR_FEATURE:
										break;
										case SET_FEATURE:
										break;
										case SET_ADDRESS:
											temp_address=SetupPacket.wValue;
											_address=ADDRESS_SET;
										/*	запишем в EP0_IN пакет ZLP тем самым подтвердим SET_ADDRESS во время status stage	*/
											Write_EPbuffer(EP0_IN,NULL,0);
										break;
										case GET_DESCRIPTOR:
											Get_Descriptor(SetupPacket.wValue,SetupPacket.wLength);
											stage0=STATUSSTAGE;
										break;
										case SET_DESCRIPTOR:
										break;
										case GET_CONFIGURATION:
										break;	
										case SET_CONFIGURATION:
											usbState=CONFIGURED;
											*CNTR &=~CNTR_WKUPM;
											*CNTR |=CNTR_SUSPM;
											*ISTR &=~ISTR_SUSP;
										
											 _SetEPAddress(1, 1);
											 _SetEPAddress(2, 2);
											/*	запишем в EP0_IN пакет ZLP тем самым подтвердим SET_CONFIGURATION во время status stage	*/
											Write_EPbuffer(EP0_IN,NULL,0);
										break;	
									}
							break;	//case REQUEST_TO_DEVICE:
							case REQUEST_TO_INTERFACE:
								switch(SetupPacket.bRequest){
									case GET_STATUS:
									break;	
									case CLEAR_FEATURE:
									break;
									case SET_FEATURE:
									break;
									case GET_INTERFACE:
									break;
									case SET_INTERFACE:
									break;	
								}
							break;//case REQUEST_TO_INTERFACE:
							case REQUEST_TO_ENDPOINT:
								switch(SetupPacket.bRequest){
									case GET_STATUS:
									break;
									case SET_FEATURE:
									case CLEAR_FEATURE:
										// Восстановим endpoint после STALL (Переведем в NACK EP1_IN)
										EP_Status((uint8_t)SetupPacket.wIndex,EP_TX_NAK);
										/*	запишем в EP0_IN пакет ZLP тем самым подтвердим  CLEAR_FEATURE во время status stage	*/
										Write_EPbuffer(EP0_IN,NULL,0);
										Endpoint_halt=1;
									break;
									case SYNCH_FRAME:
									break;
								}
							break;// 	case REQUEST_TO_ENDPOINT:
						}
			break;	//case STANDARD_REQUEST:
			case CLASS_REQUEST:
				switch(SetupPacket.bRequest){
					case GET_MAX_LUN:
						lun=0;
						Write_EPbuffer(EP0_IN,&lun,1);
					// При след. OUT транзакции (ZLP от хоста) переведем EP0_OUT из NACK в VALID
						stage0=STATUSSTAGE;
					break;	
				}
			break;//case CLASS_REQUEST:
			case VENDOR_REQUEST:
			break;//case VENDOR_REQUEST:	
		
			default:
			break;	
		
	}				
	
}
/*

*/
void IN0Process(void)
{	
	if(_address==ADDRESS_SET)
	{
		/* установка адреса устройства	*/
		_SetDADDR(temp_address | 0x80); 
		_address=ADDRESS_IDLE;
	}
	if(Endpoint_halt)//
	{
		Endpoint_halt=0;
		csw.bCSWStatus=COMMANDPASSED;
		csw.dCSWDataRecidue=0;
		Write_EPbuffer(EP1_IN,(uint8_t*)&csw,13);
		BOTState=BOTIDLE;	
	}
	
}
/*

*/
void OUT0Process(void)
{	uint16_t temp;
	
	if(stage0==STATUSSTAGE)		// Приняли ZLP от хоста --> переведем EP0_OUT в VALID -- ответим ACK
	{		
		temp=*EPxR(0)^0x3000;
		*EPxR(0)=(temp & ~0xC070)|0x80;		
		stage0=IDLESTAGE;
	}
}
/***************************************************************************************************
****************************************************************************************************/

/*
*/


/*
*/

