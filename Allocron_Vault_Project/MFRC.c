#include "LPC17xx.h"
#include "MFRC.h"
#include <stdio.h>

// You must define your RC522_CS pin here
#define RC522_CS_PORT    LPC_GPIO0
#define RC522_CS_PIN     6

// SSP instance
#define RC522_SSP        LPC_SSP1

// Helper functions for CS control
static inline void RC522_CS_LOW(void) {
    RC522_CS_PORT->FIOCLR = (1 << RC522_CS_PIN);
}

static inline void RC522_CS_HIGH(void) {
    RC522_CS_PORT->FIOSET = (1 << RC522_CS_PIN);
}

void delay(uint16_t ms){
	LPC_TIM0->PR =999;
	LPC_TIM0->TCR =0x02;
	LPC_TIM0->MR0 = 25*ms-1;
	LPC_TIM0->TCR = 0x01;
	while(LPC_TIM0->TC < LPC_TIM0->MR0);
	LPC_TIM0->TCR = 0x02;
}
void SSP1_Init(void) {
    LPC_SC->PCONP |= (1 << 10);    // Power up SSP1
    LPC_PINCON->PINSEL0 |= (2 << 14) | (2 << 16) | (2 << 18); // P0.6 SSEL1, P0.7 SCK1, P0.8 MISO1, P0.9 MOSI1
    LPC_SSP1->CR0 = 0x0707; // 8-bit, SPI Frame format, CPOL=0, CPHA=0
		LPC_GPIO0->FIODIR |= (1 << 6); // CS pin output
		LPC_GPIO0->FIOSET= (1 << 6);
		LPC_SSP1->CPSR = 8;   // Clock prescale, divide by 2
    LPC_SSP1->CR1 = 0x02;   // Master mode
    
}
uint8_t SPI_TransmitReceive(uint8_t data) {
    LPC_SSP1->DR = data;                     // Write data
    while (LPC_SSP1->SR & (1 << 4));       // Wait for RNE (Receive Not Empty)
    return LPC_SSP1->DR;    // Read received data
}
//-------------------------------------------
void Write_MFRC522(uint8_t addr, uint8_t val) {
    uint8_t addr_bits = ((addr << 1) & 0x7E);

    RC522_CS_LOW();

    SPI_TransmitReceive(addr_bits); // Send address
    SPI_TransmitReceive(val);       // Send data

    RC522_CS_HIGH();
}

//-------------------------------------------
uint8_t Read_MFRC522(uint8_t addr) {
    uint8_t addr_bits = ((addr << 1) & 0x7E) | 0x80;
    uint8_t rx_bits;

    RC522_CS_LOW();

    SPI_TransmitReceive(addr_bits); // Send address
    rx_bits = SPI_TransmitReceive(0x00); // Dummy write to receive data

    RC522_CS_HIGH();

    return rx_bits;
}

//-------------------------------------------
void SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp | mask);
}

void ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp;
    tmp = Read_MFRC522(reg);
    Write_MFRC522(reg, tmp & (~mask));
}

//-------------------------------------------
void AntennaOn(void) {
    SetBitMask(TxControlReg, 0x03);
}

void AntennaOff(void) {
    ClearBitMask(TxControlReg, 0x03);
}

void MFRC522_Reset(void) {
    Write_MFRC522(CommandReg, PCD_RESETPHASE);
		delay(50);
}

void MFRC522_Init(void) {
    MFRC522_Reset();
	
    Write_MFRC522(TModeReg, 0x80);
    Write_MFRC522(TPrescalerReg, 0xA9);
    Write_MFRC522(TReloadRegL, 0x03);
    Write_MFRC522(TReloadRegH, 0xE8);
    Write_MFRC522(TxAutoReg, 0x40);
    Write_MFRC522(ModeReg, 0x3D);

    AntennaOn();
}


//------------------------------
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType) {
    uint8_t status;
    uint16_t backBits;

    Write_MFRC522(BitFramingReg, 0x07); // TxLastBits = 7

    TagType[0] = reqMode;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

    if ((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }

    return status;
}

//------------------------------
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (command == PCD_AUTHENT) {
        irqEn = 0x12;
        waitIRq = 0x10;
    }
    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
     }

    Write_MFRC522(CommIEnReg, irqEn | 0x80);  // Enable interrupts
    ClearBitMask(CommIrqReg, 0x80);           // Clear all interrupt flags
    SetBitMask(FIFOLevelReg, 0x80);           // Flush FIFO

    Write_MFRC522(CommandReg, PCD_IDLE);      // No action, cancel current command

    // Write data to FIFO
    for (i = 0; i < sendLen; i++) {
        Write_MFRC522(FIFODataReg, sendData[i]);
    }

    // Execute command
    Write_MFRC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        SetBitMask(BitFramingReg, 0x80); // Start transmission
    }

    // Wait for completion
    i = 2000; // Max waiting time
    do {
        n = Read_MFRC522(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    ClearBitMask(BitFramingReg, 0x80); // Stop transmission

    if (i != 0) {
        if (!(Read_MFRC522(ErrorReg) & 0x1B)) {
            status = MI_OK;
            if (n & irqEn & 0x01) {
                status = MI_NOTAGERR;
            }
            if (command == PCD_TRANSCEIVE) {
                n = Read_MFRC522(FIFOLevelReg);
                lastBits = Read_MFRC522(ControlReg) & 0x07;
                if (lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }

                if (n == 0) {
                    n = 1;
                }
                if (n > MAX_LEN) {
                    n = MAX_LEN;
                }

                // Read received data from FIFO
                for (i = 0; i < n; i++) {
                    backData[i] = Read_MFRC522(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }

    return status;
}

//------------------------------
uint8_t MFRC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint16_t unLen;

    Write_MFRC522(BitFramingReg, 0x00); // TxLastBits = 0

    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;

    status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == MI_OK) {
        // Check serial number
        for (i = 0; i < 4; i++) {
            serNumCheck ^= serNum[i];
        }
        if (serNumCheck != serNum[4]) {
            status = MI_ERR;
        }
    }

    return status;
}

//------------------------------
void MFRC522_CalculateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData) {
    uint8_t i, n;

    ClearBitMask(DivIrqReg, 0x04);     // Clear CRC interrupt
    SetBitMask(FIFOLevelReg, 0x80);    // Flush FIFO

    // Write data into FIFO
    for (i = 0; i < len; i++) {
        Write_MFRC522(FIFODataReg, pIndata[i]);
    }
    Write_MFRC522(CommandReg, PCD_CALCCRC);

    // Wait for CRC calculation to complete
    i = 0xFF;
    do {
        n = Read_MFRC522(DivIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x04));

    // Read CRC result
    pOutData[0] = Read_MFRC522(CRCResultRegL);
    pOutData[1] = Read_MFRC522(CRCResultRegM);
}

//------------------------------
uint8_t MFRC522_SelectTag(uint8_t *serNum) {
    uint8_t i;
    uint8_t status;
    uint8_t size;
    uint16_t recvBits;
    uint8_t buffer[9];

    buffer[0] = PICC_SElECTTAG;
    buffer[1] = 0x70;
    for (i = 0; i < 5; i++) {
        buffer[i + 2] = serNum[i];
    }

    MFRC522_CalculateCRC(buffer, 7, &buffer[7]); // Append CRC

    status = MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 9, buffer, &recvBits);

    if ((status == MI_OK) && (recvBits == 0x18)) {
        size = buffer[0];
    } else {
        size = 0;
    }

    return size;
}

//------------------------------
void MFRC522_Halt(void) {
    uint8_t buffer[4];

    buffer[0] = PICC_HALT;
    buffer[1] = 0x00;

    MFRC522_CalculateCRC(buffer, 2, &buffer[2]);

    MFRC522_ToCard(PCD_TRANSCEIVE, buffer, 4, buffer, NULL);
}
uint8_t MFRC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t *Sectorkey, uint8_t *serNum) {
    uint8_t status;
    uint8_t buff[12];

    // First byte: command
    buff[0] = authMode;
    // Second byte: block address
    buff[1] = BlockAddr;

    // Then: 6 bytes of sector key
    for (uint8_t i = 0; i < 6; i++) {
        buff[i+2] = Sectorkey[i];
    }

    // Then: 4 bytes of card serial number (UID)
    for (uint8_t i = 0; i < 4; i++) {
        buff[i+8] = serNum[i];
    }

    status = MFRC522_ToCard(PCD_AUTHENT, buff, 12, NULL, NULL);

    if ((Read_MFRC522(Status2Reg) & 0x08) == 0) {
        status = MI_ERR;
    }

    return status;
}

uint8_t MFRC522_Read(uint8_t blockAddr, uint8_t *recvData) {
    uint8_t status;
    uint16_t unLen;

    recvData[0] = PICC_READ;
    recvData[1] = blockAddr;

    MFRC522_CalculateCRC(recvData, 2, &recvData[2]);

    status = MFRC522_ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

    if ((status != MI_OK) || (unLen != 0x90)) {
        status = MI_ERR;
    }

    return status;
}

uint8_t MFRC522_Write(uint8_t blockAddr, uint8_t *writeData) {
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;
    uint8_t buff[MAX_LEN];

    buff[0] = PICC_WRITE;
    buff[1] = blockAddr;
    MFRC522_CalculateCRC(buff, 2, &buff[2]);

    status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);

    if ((status != MI_OK) || ((recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))) {
        status = MI_ERR;
    }

    if (status == MI_OK) {
        // Now send 16 bytes data
        for (i = 0; i < 16; i++) {
            buff[i] = writeData[i];
        }

        MFRC522_CalculateCRC(buff, 16, &buff[16]);

        status = MFRC522_ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);

        if ((status != MI_OK) || ((recvBits != 4) || ((buff[0] & 0x0F) != 0x0A))) {
            status = MI_ERR;
        }
    }

    return status;
}
void MFRC522_StopCrypto1(void)
{
    // Clear MFCrypto1On bit in Status2Reg to disable encryption
    ClearBitMask(Status2Reg, 0x08);
}

