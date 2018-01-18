/**
  ******************************************************************************
  * @file    stm32f4xx_it.c 
  * @author  MCD Application Team
  * @version V1.3.0
  * @date    13-November-2013
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stm32f10x.h>
#include "usb_regs.h"
#include "usb.h"
#include "sdcard.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

extern volatile uint8_t sdInsRem;
extern volatile uint8_t sleep;
volatile USB_STATE usbState;

extern void Suspend(void);
extern void WakeUp(void);
extern volatile uint32_t	StopCondition;

extern void USB_Cable_Config(FunctionalState NewState);
extern volatile SDTranState sdTranState;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

	


/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
/*void SysTick_Handler(void)
{
	
}*/

/******************************************************************************/
/*                 STM32F4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f40xx.s/startup_stm32f427x.s).                         */
/******************************************************************************/

/**
  * @brief  This function handles EXTI_Line0_IRQHandler interrupt request.
  * @param  None
  * @retval None
  */
void EXTI0_IRQHandler(void){
	sdInsRem=1;
	EXTI_ClearITPendingBit(EXTI_Line0);
}	
 
/**
  * @brief  This function handles USB_LP_CAN1_RX0_IRQHandler interrupt request.
  * @param  None
  * @retval None
  */
void USB_LP_CAN1_RX0_IRQHandler(void){
	uint16_t val;
	/*********************************************************************************/	
	if(*ISTR & ISTR_RESET)				// reset
	{
		/*Настройка EP0*/
		*ISTR=0;
		EPx_Reset();
		//*ISTR &=~ISTR_RESET;		// Сбрасываем флаг reset прерывания
	}
/******************************************************************************/
	if(*ISTR & ISTR_CTR)			// ctr
	{
		while(*ISTR & ISTR_CTR)		
			{	
				/* Проверяем для какой EPx установился CTR	*/	
				if((*ISTR & ISTR_EP_ID)==ENDP1)
				{	/* EP1 */
					/*	очистим CTR_TX */
					val=*EPxR(1);
					*EPxR(1)=	val	& ~EP_CTR_TX & EPREG_MASK;	
					IN1_Process();
				}
				else if((*ISTR & ISTR_EP_ID)==ENDP2)
				{
					/* EP2 */
					/*	очистим CTR_TX */
					val=*EPxR(2);
					*EPxR(2)=	val	& ~EP_CTR_RX & EPREG_MASK;		
					OUT2_Process();
				}
			
				else if((*ISTR & ISTR_EP_ID)==ENDP0)
				{
					/*EP0*/
					if((*ISTR & ISTR_DIR)==0)		//DIR=0		/* DIR = 0      => IN  int */
					{
						/*	очистим CTR_TX */
						val=*EPxR(0);
						*EPxR(0)=	val	& ~EP_CTR_TX & EPREG_MASK;
						IN0Process();	
					}
					else										// DIR=1	 /* DIR = 1       => SETUP or OUT int */	
					{
						if(*EP0REG & EP_SETUP)	// Пришел токен setup?
						{
							
							/*	очистим CTR_RX */
							val=*EPxR(0);
							*EPxR(0)=	val	& ~EP_CTR_RX & EPREG_MASK;
							SETUP0Process();
							
						
						}	
						else if(*EP0REG & EP_CTR_RX)	// 
						{
							/*	очистим CTR_RX */
							val=*EPxR(0);
							*EPxR(0)=	val	& ~EP_CTR_RX & EPREG_MASK;	
							OUT0Process();
						}
					}
				}
		/*************************************************************************************/
			
		}	//	while(*ISTR & ISTR_CTR)
	}	//	if(*ISTR & ISTR_CTR)
	
/*******************************************************************************/
	if((*ISTR & ISTR_SUSP)||sleep)				// susp
	{
		Suspend();
	}
/*******************************************************************************/	
	if(*ISTR & ISTR_WKUP)				// wkup
	{
		*ISTR &=~ ISTR_WKUP;
		WakeUp();
		
	}

}

/**
  * @brief  This function handles USBWakeUp_IRQHandler interrupt request.
  * @param  None
  * @retval None
  */
void USBWakeUp_IRQHandler(void){
	//EXTI->IMR &=~EXTI_IMR_MR18;
	EXTI_ClearITPendingBit(EXTI_Line18);
}
/**
  * @brief  This function handles TIM6_IRQHandler interrupt request.
  * @param  None
  * @retval None
  */
void TIM6_IRQHandler(void){

	TIM6->SR &= ~TIM_SR_UIF; 			//Сбрасываем флаг UIF
	if(usbState!=CONFIGURED)
	{	sleep=1;
		NVIC->STIR=20;
	}
}

/**
  * @brief  This function handles SDIO_IRQHandler interrupt request.
  * @param  None
  * @retval None
  */
void SDIO_IRQHandler(void){
	
	if(SDIO->STA & (SDIO_STA_CMDREND|SDIO_STA_CCRCFAIL))	
	{
		if(SDIO->STA & SDIO_STA_CMDREND)
		{
			sdTranState=SD_READY;
			SDIO->ICR=SDIO_ICR_CMDRENDC;
		}
		else															
		{
			sdTranState=SD_ERR;
			SDIO->ICR =SDIO_ICR_CCRCFAILC;
		}
		SDIO->MASK &=~(SDIO_MASK_CMDRENDIE|SDIO_MASK_CCRCFAILIE);
	}
	else if(SDIO->STA & (SDIO_STA_DATAEND|SDIO_STA_DCRCFAIL))
	{
		SDIO->MASK &=~(SDIO_MASK_DATAENDIE|SDIO_MASK_DCRCFAILIE);	
		
		if(StopCondition)
		{
			StopCondition=0;
			if(SDIO->STA & SDIO_STA_DATAEND)
			{	
				SDIO->ICR=SDIO_ICR_DATAENDC|SDIO_ICR_DBCKENDC;
				// CPSM CMD12	STOP_TRANSMISSION
				SDIO->CMD=0;
				SDIO->ARG=0;
				SDIO->CMD |=SDIO_STOP_TRANSMISSION			// CMD12 STOP_TRANSMISSION
									|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
									|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD12	
				SDIO->MASK |=SDIO_MASK_CMDRENDIE|SDIO_MASK_CCRCFAILIE;
			}
			else
			{
				sdTranState=SD_ERR;
				SDIO->ICR=SDIO_ICR_DCRCFAILC;
			}
		}
		else
		{
			if(SDIO->STA & SDIO_STA_DATAEND)
			{
				sdTranState=SD_READY;
				SDIO->ICR=SDIO_ICR_DATAENDC|SDIO_ICR_DBCKENDC;
			}
			else
			{
				sdTranState=SD_ERR;
				SDIO->ICR=SDIO_ICR_DCRCFAILC;
			}
		}
	}
	
}
/**
  * @brief  This function handles DMA2_Channel4_5_IRQHandler interrupt request.
  * @param  None
  * @retval None
  */
void DMA2_Channel4_5_IRQHandler(void){
	
  DMA2_Channel4->CCR &=~DMA_CCR4_EN;
	DMA2->IFCR = DMA_IFCR_CGIF4;  
	if(StopCondition)
	{
		SDIO->ICR=SDIO_ICR_DATAENDC|SDIO_ICR_DBCKENDC;
		StopCondition=0;
// CPSM CMD12	STOP_TRANSMISSION
		SDIO->MASK |=SDIO_MASK_CMDRENDIE|SDIO_MASK_CCRCFAILIE;
		SDIO->CMD=0;
		SDIO->ARG=0;
		SDIO->CMD |=SDIO_STOP_TRANSMISSION			// CMD12 STOP_TRANSMISSION
							|SDIO_CMD_WAITRESP_0					// SHORT RESPONSE
							|SDIO_CMD_CPSMEN;							// CPSM IS ENABLE			// Отправляем команду CMD12
	}
	else
	{
		if(SDIO->STA & SDIO_STA_DATAEND)
		{
			if(SDIO->STA & SDIO_STA_DCRCFAIL)	
				sdTranState=SD_ERR;
			else															
			{
				sdTranState=SD_READY;
				SDIO->ICR=SDIO_ICR_DATAENDC|SDIO_ICR_DBCKENDC;
			}
		}
		
	}
}

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
