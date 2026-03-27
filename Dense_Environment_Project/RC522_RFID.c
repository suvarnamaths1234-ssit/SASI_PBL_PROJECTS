/**
 * ============================================
 * RC522 RFID Driver
 * ============================================
 */

#include "RC522_RFID.h"
#include "SSP0.h"
#include "DELAY.h"

// ============================================
// RC522 Low-Level Functions
// ============================================

void RC522_SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t temp = SSP0_Read(reg);
    SSP0_Write(reg, temp | mask);
}

void RC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t temp = SSP0_Read(reg);
    SSP0_Write(reg, temp & (~mask));
}

void RC522_ClearFIFO(void) {
    SSP0_Write(RC522_REG_FIFO_LEVEL, 0x80);
}

void RC522_reset(void) {
    SSP0_Write(RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    delay_ms(50);
}

void RC522_TX_ON(void) {
    uint8_t temp = SSP0_Read(RC522_REG_TX_CONTROL);
    if(!(temp & 0x03)) {
        SSP0_Write(RC522_REG_TX_CONTROL, temp | 0x03);
    }
    delay_ms(5);
}

void RC522_TX_off(void) {
    uint8_t temp = SSP0_Read(RC522_REG_TX_CONTROL);
    SSP0_Write(RC522_REG_TX_CONTROL, temp & ~0x03);
}

void RC522_Init(void) {
    RC522_reset();
    
    SSP0_Write(RC522_REG_TMODE, 0x8D);
    SSP0_Write(RC522_REG_TPRESCALER, 0x3E);
    SSP0_Write(RC522_REG_TRELOAD_HI, 0x00);
    SSP0_Write(RC522_REG_TRELOAD_LO, 30);
    
    SSP0_Write(RC522_REG_TXASK, 0x40);
    SSP0_Write(RC522_REG_MODE, 0x3D);
    SSP0_Write(RC522_REG_MOD_WIDTH, 0x26);
    
    SSP0_Write(RC522_REG_RFCFG, 0x70);
    SSP0_Write(RC522_REG_CWGSP, 0x3F);
    SSP0_Write(RC522_REG_MODGSP, 0x3F);
    SSP0_Write(RC522_REG_GSN, 0x88);
    
    RC522_TX_ON();
    delay_ms(10);
}

// ============================================
// RC522 ToCard Function
// ============================================

uint8_t RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, 
                     uint8_t *backData, uint16_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;
    
    switch(command) {
        case RC522_CMD_MF_AUTHENT:
            irqEn = 0x12;
            waitIRq = 0x10;
            break;
        case RC522_CMD_TRANSCEIVE:
            irqEn = 0x77;
            waitIRq = 0x30;
            break;
        default:
            break;
    }
    
    SSP0_Write(RC522_REG_COMIEN, irqEn | 0x80);
    RC522_ClearBitMask(RC522_REG_COMIRQ, 0x80);
    RC522_SetBitMask(RC522_REG_FIFO_LEVEL, 0x80);
    
    SSP0_Write(RC522_REG_COMMAND, RC522_CMD_IDLE);
    
    for(i = 0; i < sendLen; i++) {
        SSP0_Write(RC522_REG_FIFO_DATA, sendData[i]);
    }
    
    SSP0_Write(RC522_REG_COMMAND, command);
    
    if(command == RC522_CMD_TRANSCEIVE) {
        RC522_SetBitMask(RC522_REG_BIT_FRAMING, 0x80);
    }
    
    i = 2000;
    do {
        n = SSP0_Read(RC522_REG_COMIRQ);
        i--;
    } while((i != 0) && !(n & 0x01) && !(n & waitIRq));
    
    RC522_ClearBitMask(RC522_REG_BIT_FRAMING, 0x80);
    
    if(i != 0) {
        uint8_t error = SSP0_Read(RC522_REG_ERROR);
        if(!(error & 0x1B)) {
            status = MI_OK;
            
            if(n & irqEn & 0x01) {
                status = MI_NOTAGERR;
            }
            
            if(command == RC522_CMD_TRANSCEIVE) {
                n = SSP0_Read(RC522_REG_FIFO_LEVEL);
                lastBits = SSP0_Read(RC522_REG_CONTROL) & 0x07;
                
                if(lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }
                
                if(n == 0) {
                    n = 1;
                }
                if(n > 16) {
                    n = 16;
                }
                
                for(i = 0; i < n; i++) {
                    backData[i] = SSP0_Read(RC522_REG_FIFO_DATA);
                }
            }
        } else {
            status = MI_ERR;
        }
    }
    
    return status;
}

// ============================================
// RC522 High-Level Functions
// ============================================

uint8_t RC522_Request(uint8_t reqMode, uint8_t *tagType) {
    uint8_t status;
    uint16_t backBits;
    
    SSP0_Write(RC522_REG_BIT_FRAMING, 0x07);
    
    tagType[0] = reqMode;
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, tagType, 1, tagType, &backBits);
    
    if((status != MI_OK) || (backBits != 0x10)) {
        status = MI_ERR;
    }
    
    return status;
}

uint8_t RC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint16_t unLen;
    
    SSP0_Write(RC522_REG_BIT_FRAMING, 0x00);
    
    serNum[0] = PICC_CMD_SEL_CL1;
    serNum[1] = 0x20;
    
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, serNum, 2, serNum, &unLen);
    
    if(status == MI_OK) {
        if(unLen == 0x28) {
            for(i = 0; i < 4; i++) {
                serNumCheck ^= serNum[i];
            }
            if(serNumCheck != serNum[4]) {
                status = MI_ERR;
            }
        } else {
            status = MI_ERR;
        }
    }
    
    return status;
}

uint8_t RC522_SelectTag(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint16_t recvBits;
    uint8_t buffer[9];
    
    buffer[0] = PICC_CMD_SEL_CL1;
    buffer[1] = 0x70;
    
    for(i = 0; i < 5; i++) {
        buffer[i + 2] = serNum[i];
    }
    
    RC522_CalculateCRC(buffer, 7, &buffer[7]);
    
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, buffer, 9, buffer, &recvBits);
    
    if((status == MI_OK) && (recvBits == 0x18)) {
        status = MI_OK;
    } else {
        status = MI_ERR;
    }
    
    return status;
}

uint8_t RC522_Auth(uint8_t authMode, uint8_t blockAddr, uint8_t *key, uint8_t *serNum) {
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;
    uint8_t buffer[12];
    
    buffer[0] = authMode;
    buffer[1] = blockAddr;
    
    for(i = 0; i < 6; i++) {
        buffer[i + 2] = key[i];
    }
    
    for(i = 0; i < 4; i++) {
        buffer[i + 8] = serNum[i];
    }
    
    status = RC522_ToCard(RC522_CMD_MF_AUTHENT, buffer, 12, buffer, &recvBits);
    
    if(status != MI_OK) {
        return MI_ERR;
    }
    
    if(!(SSP0_Read(RC522_REG_STATUS2) & 0x08)) {
        status = MI_ERR;
    }
    
    return status;
}

uint8_t RC522_Read(uint8_t blockAddr, uint8_t *recvData) {
    uint8_t status;
    uint16_t unLen;
    
    recvData[0] = PICC_CMD_MF_READ;
    recvData[1] = blockAddr;
    
    RC522_CalculateCRC(recvData, 2, &recvData[2]);
    
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, recvData, 4, recvData, &unLen);
    
    if((status != MI_OK) || (unLen != 0x90)) {
        status = MI_ERR;
    }
    
    return status;
}

uint8_t RC522_Write(uint8_t blockAddr, uint8_t *writeData) {
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;
    uint8_t buffer[18];
    
    buffer[0] = PICC_CMD_MF_WRITE;
    buffer[1] = blockAddr;
    
    RC522_CalculateCRC(buffer, 2, &buffer[2]);
    
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, buffer, 4, buffer, &recvBits);
    
    if((status != MI_OK) || (recvBits != 4) || ((buffer[0] & 0x0F) != 0x0A)) {
        status = MI_ERR;
    }
    
    if(status == MI_OK) {
        for(i = 0; i < 16; i++) {
            buffer[i] = writeData[i];
        }
        
        RC522_CalculateCRC(buffer, 16, &buffer[16]);
        
        status = RC522_ToCard(RC522_CMD_TRANSCEIVE, buffer, 18, buffer, &recvBits);
        
        if((status != MI_OK) || (recvBits != 4) || ((buffer[0] & 0x0F) != 0x0A)) {
            status = MI_ERR;
        }
    }
    
    return status;
}

void RC522_CalculateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData) {
    uint8_t i, n;
    
    RC522_ClearBitMask(RC522_REG_DIVIRQ, 0x04);
    RC522_SetBitMask(RC522_REG_FIFO_LEVEL, 0x80);
    
    for(i = 0; i < len; i++) {
        SSP0_Write(RC522_REG_FIFO_DATA, pIndata[i]);
    }
    
    SSP0_Write(RC522_REG_COMMAND, RC522_CMD_CALC_CRC);
    
    i = 0xFF;
    do {
        n = SSP0_Read(RC522_REG_DIVIRQ);
        i--;
    } while((i != 0) && !(n & 0x04));
    
    pOutData[0] = SSP0_Read(RC522_REG_CRC_RESULT_L);
    pOutData[1] = SSP0_Read(RC522_REG_CRC_RESULT_H);
}

void RC522_Halt(void) {
    uint16_t unLen;
    uint8_t buffer[4];
    
    buffer[0] = PICC_CMD_HLTA;
    buffer[1] = 0;
    
    RC522_CalculateCRC(buffer, 2, &buffer[2]);
    
    RC522_ToCard(RC522_CMD_TRANSCEIVE, buffer, 4, buffer, &unLen);
}
