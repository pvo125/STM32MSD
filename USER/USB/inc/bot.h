
#ifndef __BOT_H
#define __BOT_H

#include <stm32f10x.h>


#define	BOTIDLE 				0
#define	BOTDATA_IN	 		1
#define	BOTDATA_OUT	 		2
#define	BOTLAST_DATA_IN 3
#define	SEND_CSW		 		4

#define COMMANDPASSED		0
#define COMMANDFAILED		1
#define PHASEERROR			2


#define SCSI_TESTUNITREADY				0x00
#define SCSI_REQUESTSENSE					0x03
#define SCSI_INQUIRY							0x12
#define SCSI_ALLOW_MEDIUM_REMOVAL	0x1E
#define SCSI_READFORMATCAPACITY		0x23
#define SCSI_READCAPACITY10				0x25
#define SCSI_READ10								0x28
#define SCSI_MODESENSE6						0x1A
#define SCSI_START_STOP_UNIT			0x1B

#define SCSI_WRITE10							0x2A
#define SCSI_VERIFY10             0x2F


#define NO_SENSE		                    0
#define RECOVERED_ERROR		              1
#define NOT_READY		                    2
#define MEDIUM_ERROR		                3
#define HARDWARE_ERROR		              4
#define ILLEGAL_REQUEST		              5
#define UNIT_ATTENTION		              6
#define DATA_PROTECT		                7
#define BLANK_CHECK		                  8
#define VENDOR_SPECIFIC		              9
#define COPY_ABORTED		                10
#define ABORTED_COMMAND		              11
#define VOLUME_OVERFLOW		              13
#define MISCOMPARE		                  14				

#define INVALID_COMMAND                             0x20
#define INVALID_FIELED_IN_COMMAND                   0x24
#define PARAMETER_LIST_LENGTH_ERROR                 0x1A
#define INVALID_FIELD_IN_PARAMETER_LIST             0x26
#define ADDRESS_OUT_OF_RANGE                        0x21
#define MEDIUM_NOT_PRESENT 			    								0x3A
#define MEDIUM_HAVE_CHANGED			    								0x28


typedef struct{
	uint32_t dCBWSignature;
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWBLength;
	uint8_t CBWCB[16];
	uint8_t RESERVED;
}CBW;

typedef struct{
	
	uint32_t dCSWSignature;
	uint32_t dCSWTag;
	uint32_t dCSWDataRecidue;
	uint8_t  bCSWStatus;
}CSW;
















#endif
