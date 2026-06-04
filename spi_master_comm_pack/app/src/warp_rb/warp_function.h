/*
 * app_ton.h
 *
 * New role:
 * - adapter between RCSA packet layer and new i2c ring buffer path
 */

#ifndef SRC_APPLY_WARP_FUNCTION_H_
#define SRC_APPLY_WARP_FUNCTION_H_

#include <stdint.h>
#include <stdbool.h>

#include "stm32f1xx_hal.h"
//#include "stm32f1xx_hal_uart.h"
#include "../Packet_RCSA/SerialComm.h"
#include "../Packet_RCSA/UDPPacket.h"


#ifdef __cplusplus
extern "C" {
#endif

void InitializeI2CComm(UARTReadFunction r_func, UARTWriteFunction w_func);
void RunReceiveI2CComm(void);
uint16_t i2c_peek_one_packet(uint16_t tx_buf_size);
bool i2c_extract_one_packet(uint8_t *tx_buf, uint16_t tx_buf_size, uint16_t *out_len);


UDPPacket* GetReceivedUDPPacketI2CComm(void);
void SendUDPPacketI2CComm(UDPPacket* packet);

//void I2CWriteByte(uint8_t byte);
//bool I2CReadByte(uint8_t* byte);

void I2CMasterWriteByte(uint8_t byte);
bool I2CMasterReadByte(uint8_t* byte);

void I2CSlaveWriteByte(uint8_t byte);
bool I2CSlaveReadByte(uint8_t* byte);


#ifdef __cplusplus
}
#endif

#endif /* SRC_APPLY_WARP_FUNCTION_H_ */
