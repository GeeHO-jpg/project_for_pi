/*
 * SerialComm.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_SERIALCOMM_H_
#define INC_PACKET_HEADER_SERIALCOMM_H_

#include <stdlib.h>
#include <stdbool.h>
#include "Commands.h"
#include "CircularBuffer.h"
#include "UDPPacket.h"
#include "IDNumber.h"
//#include "crc_32.h"

// Function Pointer Types
typedef bool (*UARTReadFunction)(uint8_t *byte);
typedef void (*UARTWriteFunction)(uint8_t);

typedef struct {
    CircularBuffer* recv_buffer;
    UDPPacket* incomplete_packet;
    UDPPacket* complete_packet;
    UARTReadFunction Read;
    UARTWriteFunction Write;
} SerialComm;

#ifdef __cplusplus
extern "C" {
#endif

bool IsOperableSerialComm(SerialComm* serial);
SerialComm* CreateSerialComm(UARTReadFunction r_func, UARTWriteFunction w_func);

void FreeReceiveBufferSerialComm(SerialComm* serial);
void FreeIncompletePacketSerialComm(SerialComm* serial);
void FreeCompletePacketSerialComm(SerialComm* serial);
void FreeSerialComm(SerialComm* serial);

void RunReceiveSerialComm(SerialComm* serial);
void ProcessBufferSerialComm(SerialComm* serial);
void FinalizeIncompletePacketSerialComm(SerialComm* serial, uint32_t crc_recv);

UDPPacket* GetCompletePacketSerialComm(SerialComm* serial);

void SendBytesSerialComm(SerialComm* serial, uint8_t* send_bytes, size_t send_size);
void SendUDPPacketSerialComm(SerialComm* serial, UDPPacket* packet);

#ifdef __cplusplus
}
#endif

#endif /* INC_PACKET_HEADER_SERIALCOMM_H_ */
