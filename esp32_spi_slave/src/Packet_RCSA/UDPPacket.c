/*
 * UDPPacket.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#include "UDPPacket.h"

extern void SendUDPPacketWifiCommUdp(UDPPacket* packet);

UDPPacket* CreateUDPPacket(UDPPacketHeader* header)
{
    if (header == NULL) {
        return NULL;
    }

    UDPPacket* packet = (UDPPacket*)malloc(sizeof(UDPPacket));
    if (packet == NULL) {
        return NULL;
    }

    packet->header = header;
    packet->payload = NULL;
    packet->payload_tail_index = 0U;

    if (header->payload_size > 0U) {
        packet->payload = (uint8_t*)malloc(sizeof(uint8_t) * header->payload_size);
        if (packet->payload == NULL) {
            free(packet);
            return NULL;
        }
    }

    return packet;
}

void FreeUDPPacket(UDPPacket* packet)
{
    if (packet == NULL) {
        return;
    }

    if (packet->header != NULL) {
        free(packet->header);
        packet->header = NULL;
    }

    if (packet->payload != NULL) {
        free(packet->payload);
        packet->payload = NULL;
    }

    free(packet);
}

bool IsOperableUDPPacket(UDPPacket* packet)
{
    return packet != NULL && packet->header != NULL;
}

bool IsPayloadCompletedUDPPacket(UDPPacket* packet)
{
    return packet->payload_tail_index >= packet->header->payload_size;
}

bool AppendBytePayloadUDPPacket(UDPPacket* packet, uint8_t in_byte)
{
    if (IsOperableUDPPacket(packet) && !IsPayloadCompletedUDPPacket(packet)) {
        packet->payload[packet->payload_tail_index] = in_byte;
        packet->payload_tail_index++;
        return true;
    }

    return false;
}

bool AttachCompletedPayload(UDPPacket* packet, const uint8_t* payload_byte, uint16_t payload_size)
{
    if (IsOperableUDPPacket(packet) && !IsPayloadCompletedUDPPacket(packet)) {
        if (packet->header->payload_size == payload_size && payload_byte != NULL) {
            memcpy(packet->payload, payload_byte, payload_size);
            packet->payload_tail_index = payload_size;
            return true;
        }
    } else if (payload_size == 0U) {
        return true;
    }

    return false;
}

bool SendUdpWithPayloadWifi_buff(uint16_t id, uint16_t cmd, const uint8_t* payload, uint16_t payload_len)
{
    if (payload_len > 0U && payload == NULL) {
        return false;
    }

    UDPPacketHeader* hdr = CreateUDPPacketHeader(id, cmd, payload_len);
    if (hdr == NULL) {
        return false;
    }

    UDPPacket* pkt = CreateUDPPacket(hdr);
    if (pkt == NULL) {
        free(hdr);
        return false;
    }

    if (payload_len > 0U) {
        AttachCompletedPayload(pkt, payload, payload_len);
    }

    SendUDPPacketWifiCommUdp(pkt);
    FreeUDPPacket(pkt);

    return true;
}
