/* Includes ------------------------------------------------------------------*/
#include <stm32f10x.h>
#include "stm32f10x_conf.h"
#include "usb_regs.h"
#include "usb.h"
#include "sdcard.h"
#include "my_sdcard.h"
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

volatile uint8_t sleep=0;
extern volatile USB_STATE usbState;

void Suspend(void);
void WakeUp(void);
extern void SD_LowLevel_Init(void);

uint32_t sdBlockCNT;
uint32_t sdBlockLEN;
volatile uint8_t sdInsRem=0;


SD_CardInfo sdcardinfo;
SD_Error sd_error=SD_ERROR;
/* Private function prototypes -----------------------------------------------*/
void USB_Init(void);
void USB_Cable_Config(FunctionalState NewState);
void Init_Periph(void);
SD_Error sdCardInitialization(void);

int main(void){
	
  USB_Cable_Config(DISABLE);
	Init_Periph();
	
	PWR->CR &=~PWR_CR_PDDS;		//Enter Stop mode when the CPU enters Deepsleep.
	PWR->CR |=PWR_CR_LPDS;		//Voltage regulator in low-power mode during Stop mode
	DBGMCU->CR|=DBGMCU_CR_DBG_STOP;
	
	if(!(GPIOE->IDR & GPIO_IDR_IDR0))
		sd_error=sdCardInitialization();

	USB_Init();
  USB_Cable_Config(ENABLE);
	
	

while(1)
	{
		if(sdInsRem)
		{
			SysTick->LOAD=16000000;
			SysTick->VAL=0;
			while(!(SysTick->CTRL&SysTick_CTRL_COUNTFLAG)){}
			if(!(GPIOE->IDR & GPIO_IDR_IDR0))
			{
				sd_error=sdCardInitialization();
			}			
			else
			{
				sd_error=SD_ERROR;
				SDIO_DeInit();
			}
			sdInsRem=0;
		}
	__WFI();
	}
}

/*
*/
void Init_Periph(void){
	
	GPIO_InitTypeDef GPIO_InitStruct;
	EXTI_InitTypeDef EXTI_InitStruct;
		
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC|
												RCC_AHBPeriph_DMA2|
												RCC_AHBPeriph_SDIO,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB|
												 RCC_APB1Periph_PWR|		
												 RCC_APB1Periph_BKP|
												 RCC_APB1Periph_TIM6, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO|
												 RCC_APB2Periph_GPIOA|
												 RCC_APB2Periph_GPIOB|
												 RCC_APB2Periph_GPIOC|
												 RCC_APB2Periph_GPIOD|
												 RCC_APB2Periph_GPIOE,ENABLE);
	
	RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);				//72MHz /1.5=48 MHz
	
// Настроим SysTick сбросим флаг CLKSOURCE выберем источник тактирования AHB/8
	SysTick->CTRL &=~SysTick_CTRL_CLKSOURCE;
	SysTick->CTRL |=SysTick_CTRL_ENABLE_Msk;	
	SysTick->LOAD=16000000;
	/* Настроим таймер для выключения при ложном WakeUp								   	  			*/	
	/* 										нициализация таймера TIM6								    					 */	
	/*****************************************************************************/
	TIM6->PSC = 36000 - 1; 			// Настраиваем делитель что таймер тикал 2000 раз в секунду
	TIM6->ARR = 10000 ; 				// Чтоб прерывание случалось через 10000/2000=5 секунд
	TIM6->DIER |= TIM_DIER_UIE; //разрешаем прерывание от таймера 
	TIM6->EGR = TIM_EGR_UG;	  	//генерируем "update event". ARR и PSC грузятся из предварительного в теневой регистр. 
	TIM6->SR&=~TIM_SR_UIF; 			//Сбрасываем флаг UIF
	NVIC_ClearPendingIRQ(TIM6_IRQn);
	TIM6->CR1 |=TIM_CR1_CEN;
	
	SCB->CCR|=SCB_CCR_USERSETMPEND_Msk;
	
	/*Set PA11,12 as IN - USB_DM,DP*/
  GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStruct);

 /* Configure the EXTI line 18 connected internally to the USB IP */
  EXTI_InitStruct.EXTI_Line = EXTI_Line18;
  EXTI_InitStruct.EXTI_Mode=EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
  EXTI_InitStruct.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStruct);
	EXTI_ClearITPendingBit(EXTI_Line18);
		
//	SDIO card insert  PE0		
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_0;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_2MHz;			
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_Init(GPIOE,&GPIO_InitStruct);	
	
	EXTI_ClearITPendingBit(EXTI_Line0);
  EXTI_InitStruct.EXTI_Line = EXTI_Line0;
  EXTI_InitStruct.EXTI_Mode=EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
  EXTI_InitStruct.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStruct);
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource0);
	
	EXTI_ClearITPendingBit(EXTI_Line0);
	NVIC_ClearPendingIRQ(EXTI0_IRQn);
	
	NVIC_SetPriority(EXTI0_IRQn,2);
	NVIC_EnableIRQ(EXTI0_IRQn);
		
	NVIC_SetPriority(TIM6_IRQn,2);
	NVIC_SetPriority(DMA2_Channel4_5_IRQn,0);
	NVIC_SetPriority(SDIO_IRQn,0);
	NVIC_EnableIRQ(DMA2_Channel4_5_IRQn);						//Разрешение DMA2_Channel4_5_IRQn прерывания
	NVIC_EnableIRQ(SDIO_IRQn);
	NVIC_EnableIRQ(TIM6_IRQn);	
	
#if 1
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|
														 GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10/*GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15*/;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AIN;
  GPIO_Init(GPIOA, &GPIO_InitStruct);	
	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|/*GPIO_Pin_3|GPIO_Pin_4|*/GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|
														 GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStruct);			

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|
														 /*GPIO_Pin_8||GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|*/GPIO_Pin_14|GPIO_Pin_15;
	GPIO_Init(GPIOC, &GPIO_InitStruct);	
	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1/*|GPIO_Pin_2*/|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|
														 GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
	GPIO_Init(GPIOD, &GPIO_InitStruct);
	
	GPIO_InitStruct.GPIO_Pin =/*GPIO_Pin_0|*/GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7|
														 GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStruct);
#endif	
}
/*
*/
void USB_Init(void){	
	/*Настройка модуля USB*/
	
	*CNTR &=~CNTR_PDWN;																// Включаем аналоговый траннсивер 
	SysTick->LOAD=90;
	SysTick->VAL=0;
	while(!(SysTick->CTRL&SysTick_CTRL_COUNTFLAG)){} // while добавить задержку на включение tstartup=10us
		
	*CNTR &=~CNTR_FRES;															 // Снимаем reset с модуля.
	
	*CNTR |=CNTR_CTRM|CNTR_RESETM/*|CNTR_WKUPM*/;	// Включаем маски прерываний RES WKUP CTR
	
	*BTABLE=0;	// 	Buffer description table  0x4000 6000
	NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn,1);
	NVIC_SetPriority(USBWakeUp_IRQn,0);
	NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);			// Разрешаем прерывание для USB
	NVIC_EnableIRQ(USBWakeUp_IRQn);
}

/*
*/
void USB_Cable_Config(FunctionalState NewState){
	GPIO_InitTypeDef GPIO_InitStruct;		
	
	if(NewState==ENABLE)
	{
		GPIO_InitStruct.GPIO_Pin = USB_DISCONNECT_PIN;
		GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
		GPIO_Init(USB_DISCONNECT_PORT, &GPIO_InitStruct);
		
		GPIO_ResetBits(USB_DISCONNECT_PORT, USB_DISCONNECT_PIN);
	}
	else
	{
		GPIO_InitStruct.GPIO_Pin = USB_DISCONNECT_PIN;
		GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
		GPIO_Init(USB_DISCONNECT_PORT, &GPIO_InitStruct);
	}

}
/*
*/
SD_Error sdCardInitialization(void){
		uint16_t mult;
		
		if(SD_Init()!=SD_OK)
			return SD_ERROR;
		if(SD_GetCardInfo(&sdcardinfo)!=SD_OK)
			return SD_ERROR;
		if(SD_SelectDeselect((u32)sdcardinfo.RCA<<16)!=SD_OK)	
			return SD_ERROR;
		if(SD_EnableWideBusOperation(SDIO_BusWide_4b)!=SD_OK)	
			return SD_ERROR;
		SD_SetDeviceMode(SD_DMA_MODE);
		mult=1<<(sdcardinfo.SD_csd.DeviceSizeMul+2);
		sdBlockCNT=(sdcardinfo.SD_csd.DeviceSize+1)*mult;
		sdBlockLEN=SDSECTOR_SIZE;
		SDCmdSetBlockLen(sdBlockLEN);
		return SD_OK;				
}

/*
*/
void Suspend(void){
	GPIO_InitTypeDef GPIO_InitStruct;
			
	USB_Cable_Config(DISABLE);
	
	usbState=SUSPENDED;
	TIM6->CR1 &=~TIM_CR1_CEN;
		
	EXTI->IMR &=~EXTI_IMR_MR0;
			
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12;
  GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AIN;
  GPIO_Init(GPIOC, &GPIO_InitStruct);	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
  GPIO_Init(GPIOD, &GPIO_InitStruct);	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
  GPIO_Init(GPIOE, &GPIO_InitStruct);
	
	*CNTR |=CNTR_WKUPM;
	*CNTR &=~CNTR_SUSPM;
	
	*CNTR |=CNTR_FRES;
	*CNTR &=~CNTR_FRES;
	while((*ISTR & ISTR_RESET)==0) {}
	*ISTR=0;	
	
	*CNTR |=CNTR_FSUSP;
	*CNTR |=CNTR_LPMODE;
 	
	SysTick->LOAD=225000;			// delay 225000/9000000=25ms
	SysTick->VAL=0;
	while(!(SysTick->CTRL&SysTick_CTRL_COUNTFLAG)){}

	SCB->SCR |= (uint32_t)((uint32_t)SCB_SCR_SLEEPDEEP);
		__WFI();
	SCB->SCR &= (uint32_t)~((uint32_t)SCB_SCR_SLEEPDEEP);
	
	*CNTR &=~CNTR_FSUSP;
	*CNTR &=~CNTR_LPMODE;		
	*ISTR &=~ ISTR_SUSP;
}
/*
*/
void WakeUp(void){
	GPIO_InitTypeDef GPIO_InitStruct;
		
	SystemInit();
	//	SDIO card insert  PE0		
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_0;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_2MHz;			
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_Init(GPIOE,&GPIO_InitStruct);
	
	EXTI->IMR |=EXTI_IMR_MR0;
	EXTI_ClearITPendingBit(EXTI_Line0);
	
	sd_error=sdCardInitialization();
	if(sd_error==SD_ERROR)
		SDIO_DeInit();
	
	TIM6->CNT=0;
	TIM6->CR1 |=TIM_CR1_CEN;
	sleep=0;
	
	USB_Cable_Config(ENABLE);

}

/*
*/
void assert_failed(uint8_t* file, uint32_t line)
{
	while(1){};
}

