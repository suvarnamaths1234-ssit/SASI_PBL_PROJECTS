#ifndef __MFRC_H
#define __MFRC_H

#include <stdint.h>

// MFRC522 Commands
#define PCD_IDLE            0x00
#define PCD_AUTHENT         0x0E
#define PCD_RECEIVE         0x08
#define PCD_TRANSMIT        0x04
#define PCD_TRANSCEIVE      0x0C
#define PCD_RESETPHASE      0x0F
#define PCD_CALCCRC         0x03

// Mifare_One card command word
#define PICC_REQIDL         0x26
#define PICC_REQALL         0x52
#define PICC_ANTICOLL       0x93
#define PICC_SElECTTAG      0x93
#define PICC_AUTHENT1A      0x60
#define PICC_AUTHENT1B      0x61
#define PICC_READ           0x30
#define PICC_WRITE          0xA0
#define PICC_DECREMENT      0xC0
#define PICC_INCREMENT      0xC1
#define PICC_RESTORE        0xC2
#define PICC_TRANSFER       0xB0
#define PICC_HALT           0x50

// MFRC522 Registers
#define CommandReg          0x01
#define CommIEnReg          0x02
#define DivIrqReg           0x05
#define CommIrqReg          0x04
#define ErrorReg            0x06
#define Status1Reg          0x07
#define Status2Reg          0x08
#define FIFODataReg         0x09
#define FIFOLevelReg        0x0A
#define ControlReg          0x0C
#define BitFramingReg       0x0D
#define ModeReg             0x11
#define TxControlReg        0x14
#define CRCResultRegM       0x21
#define CRCResultRegL       0x22
#define TModeReg            0x2A
#define TPrescalerReg       0x2B
#define TReloadRegL         0x2D
#define TReloadRegH         0x2C
#define TxAutoReg           0x15

// Misc constants
#define MAX_LEN             16
#define MI_OK               0
#define MI_NOTAGERR         1
#define MI_ERR              2

const char* getNameFromUID(uint8_t *uid);



// Function prototypes
void delay(uint16_t ms);
void SSP1_Init(void);
void MFRC522_Init(void);
void Write_MFRC522(uint8_t addr, uint8_t val);
uint8_t Read_MFRC522(uint8_t addr);
void SetBitMask(uint8_t reg, uint8_t mask);
void ClearBitMask(uint8_t reg, uint8_t mask);




uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen);
uint8_t MFRC522_Anticoll(uint8_t *serNum);
void MFRC522_CalculateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData);
uint8_t MFRC522_SelectTag(uint8_t *serNum);
void MFRC522_Halt(void);
uint8_t MFRC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t *Sectorkey, uint8_t *serNum);
uint8_t MFRC522_Read(uint8_t blockAddr, uint8_t *recvData);
uint8_t MFRC522_Write(uint8_t blockAddr, uint8_t *writeData);
void MFRC522_StopCrypto1(void);

#endif
