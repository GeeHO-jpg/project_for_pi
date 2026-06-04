/*
 * UDPPacket.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_UDPPACKET_H_
#define INC_PACKET_HEADER_UDPPACKET_H_

#include "UDPPacketHeader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UDPPacketHeader* header;
    uint8_t* payload;
    size_t payload_tail_index;
} UDPPacket;

UDPPacket* CreateUDPPacket(UDPPacketHeader* header);
void FreeUDPPacket(UDPPacket* packet);

bool IsOperableUDPPacket(UDPPacket* packet);
bool IsPayloadCompletedUDPPacket(UDPPacket* packet);
bool AppendBytePayloadUDPPacket(UDPPacket* packet, uint8_t in_byte);
bool AttachCompletedPayload(UDPPacket* packet, const uint8_t* payload_byte, uint16_t payload_size);
bool SendUdpWithPayloadWifi_buff(uint16_t id, uint16_t cmd, const uint8_t* payload, uint16_t payload_len);

#ifdef __cplusplus
}
#endif

#endif /* INC_PACKET_HEADER_UDPPACKET_H_ */
