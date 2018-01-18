#ifndef __MY_SDCARD_H
#define __MY_SDCARD_H

#include "stm32f10x.h"
#include "sdcard.h"

#define SDSECTOR_SIZE    0x200
#define READY_FOR_DATA 		0x00000100
#define SDIO_STATIC_FLAGS               ((u32)0x000005FF)



SD_Error SDCmdSetSendStatus(uint32_t rca);
SD_Error SDCmdSetBlockLen(uint16_t len);
SD_Error StartReadBlockDMA(uint32_t addr,uint8_t * buffer);
SD_Error StartReadMultiBlocksDMA(uint32_t addr,uint8_t * buffer,uint32_t count);
SD_Error StartWriteBlockDMA(uint32_t addr,uint8_t *buffer);
SD_Error StartWriteMultiBlocksDMA(uint32_t addr,uint8_t *buffer,uint32_t count);

#endif
