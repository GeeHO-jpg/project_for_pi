/*
 * SerialComm.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */


#include "SerialComm.h"
#include "crc_32.h"


//#include "debug.h"
/* -------- DEBUG SEND BUFFER -------- */

uint8_t debug_send_bytes[15];
uint16_t debug_send_size = 0;

/* -------- CRC DEBUG -------- */

volatile uint32_t debug_crc_recv = 0;
volatile uint32_t debug_crc_calc = 0;
volatile uint32_t debug_crc_ok   = 0;
volatile uint32_t debug_crc_fail = 0;

bool IsOperableSerialComm(SerialComm* serial)
{
    return serial != NULL && IsOperableCircularBuffer(serial->recv_buffer);
}

SerialComm* CreateSerialComm(UARTReadFunction r_func, UARTWriteFunction w_func)
{
    SerialComm* serial = (SerialComm *)malloc(sizeof(SerialComm));
    if (serial == NULL)
        return NULL;

    serial->recv_buffer = CreateCircularBuffer(UDPPACKETHEADER_SIZE);
    if (serial->recv_buffer == NULL)
    {
        free(serial);
        serial = NULL;
        return NULL;
    }

    serial->incomplete_packet = NULL;
    serial->complete_packet = NULL;

    serial->Read = r_func;
    serial->Write = w_func;

    return serial;
}

void FreeReceiveBufferSerialComm(SerialComm* serial)
{
    FreeCircularBuffer(serial->recv_buffer);
    serial->recv_buffer = NULL;
}

void FreeIncompletePacketSerialComm(SerialComm* serial)
{
    FreeUDPPacket(serial->incomplete_packet);
    serial->incomplete_packet = NULL;
}

void FreeCompletePacketSerialComm(SerialComm* serial)
{
    FreeUDPPacket(serial->complete_packet);
    serial->complete_packet = NULL;
}

void FreeSerialComm(SerialComm* serial)
{
    if (serial == NULL)
        return;

    FreeReceiveBufferSerialComm(serial);
    FreeIncompletePacketSerialComm(serial);
    FreeCompletePacketSerialComm(serial);

    free(serial);
}

void RunReceiveSerialComm(SerialComm* serial)
{
    if (!IsOperableSerialComm(serial))
        return;

    for (size_t i = 0; i < UDPPAYLOAD_MAX_SIZE; i++)
    {
        uint8_t read_byte;

        if (!serial->Read(&read_byte))
            break;

        if (!IsOperableUDPPacket(serial->incomplete_packet))
        {
            if (AppendCircularBuffer(serial->recv_buffer, read_byte))
                ProcessBufferSerialComm(serial);
        }
        else
        {
            if (!AppendBytePayloadUDPPacket(serial->incomplete_packet, read_byte))
                FreeIncompletePacketSerialComm(serial);
        }

        if (IsOperableUDPPacket(serial->incomplete_packet) &&
            IsPayloadCompletedUDPPacket(serial->incomplete_packet))
        {
            uint8_t crc_bytes[sizeof(uint32_t)];
            uint32_t crc_recv;

            size_t got = 0;

            while (got < sizeof(uint32_t))
            {
                if (!serial->Read(&crc_bytes[got]))
                    break;

                got++;
            }

            if (got == sizeof(uint32_t))
            {
                memcpy(&crc_recv, crc_bytes, sizeof(crc_recv));
                debug_crc_recv = crc_recv;

                FinalizeIncompletePacketSerialComm(serial, crc_recv);
            }

            break;
        }
    }
}

void ProcessBufferSerialComm(SerialComm* serial)
{
    // Check the first byte to see if it's part of the packet header
    uint8_t byte_1 = PeekCircularBufferHeadByte(serial->recv_buffer);
    if (!IsPacketHeaderFirstByte(byte_1))
        return;

    // Read buffer to check for packet header signature
    uint8_t* buffer = ReadCircularBuffer(serial->recv_buffer);
    if (buffer == NULL)
        return;

    if (!IsPacketHeaderSignature(buffer))
    {
        free(buffer);  // Free buffer if signature is invalid
        return;
    }

    // Extract UDP packet header
    UDPPacketHeader* header = GetUDPPacketHeader(buffer, serial->recv_buffer->size);
    free(buffer);  // Free buffer after extracting header
    if (header == NULL)
        return;

    // Validate the extracted header
    if (!IsValidIDNumber(header->id) || !IsValidUDPCommand(header->cmd) || header->payload_size > UDPPAYLOAD_MAX_SIZE)
    {
        free(header);  // Invalid header, free it and return
        return;
    }

    // Clean up any incomplete packets and create a new one
    FreeIncompletePacketSerialComm(serial);
    serial->incomplete_packet = CreateUDPPacket(header);

    if (serial->incomplete_packet != NULL)
        ResetCircularBuffer(serial->recv_buffer);  // Only reset buffer if the packet was created
    else
        free(header);   // If packet creation fails, free header
}

void FinalizeIncompletePacketSerialComm(SerialComm* serial, uint32_t crc_recv)
{
    uint8_t* header_bytes = ToBytesUDPPacketHeader(serial->incomplete_packet->header);
    if (header_bytes == NULL) {
        FreeIncompletePacketSerialComm(serial);
        return;
    }

    rcsa_crc32_t crc_calc;
    rcsa_crc32_init(&crc_calc);
    rcsa_crc32_update(&crc_calc, header_bytes, UDPPACKETHEADER_SIZE);
    if (serial->incomplete_packet->header->payload_size > 0)
        rcsa_crc32_update(&crc_calc, serial->incomplete_packet->payload, serial->incomplete_packet->header->payload_size);

    if (rcsa_crc32_get(&crc_calc) == crc_recv) {
        FreeCompletePacketSerialComm(serial);
        serial->complete_packet = serial->incomplete_packet;
        serial->incomplete_packet = NULL;
    }

    free(header_bytes);
    FreeIncompletePacketSerialComm(serial);
}

UDPPacket* GetCompletePacketSerialComm(SerialComm* serial)
{
    if (!IsOperableSerialComm(serial))
        return NULL;

    if (!IsOperableUDPPacket(serial->complete_packet) || !IsPayloadCompletedUDPPacket(serial->complete_packet))
    {
        // FreeUDPPacket(serial->complete_packet);
        // serial->complete_packet = NULL;
        FreeCompletePacketSerialComm( serial);
        return NULL;
    }

    UDPPacket* temp = serial->complete_packet;
    serial->complete_packet = NULL;
    return temp;
}

// void SendBytesSerialComm(SerialComm* serial, uint8_t* send_bytes, size_t send_size)
// {
//     if (!IsOperableSerialComm(serial) || send_bytes == NULL || send_size < 1)
//         return;

//     for (size_t i=0; i < send_size; i++)
//     {
//         serial->Write(send_bytes[i]);
//     }
// }

void SendBytesSerialComm(SerialComm* serial, uint8_t* send_bytes, size_t send_size)
{
    if (!IsOperableSerialComm(serial) || send_bytes == NULL || send_size < 1)
        return;

    /* ---- DEBUG COPY ---- */
    debug_send_size = send_size;

    size_t debug_copy_size = send_size;

    if(debug_copy_size > sizeof(debug_send_bytes))
        debug_copy_size = sizeof(debug_send_bytes);

    memcpy(debug_send_bytes, send_bytes, debug_copy_size);

    /* ---- SEND REAL DATA ---- */
    for (size_t i = 0; i < send_size; i++)
    {
        serial->Write(send_bytes[i]);
    }
}

void SendUDPPacketSerialComm(SerialComm* serial, UDPPacket* packet)
{

    if (!IsOperableSerialComm(serial) || !IsOperableUDPPacket(packet) || !IsPayloadCompletedUDPPacket(packet))
        return;


    uint8_t* header_bytes = ToBytesUDPPacketHeader(packet->header);
    // Debug_Print("header_bytes");
    // printArray(header_bytes , 9);
    if (header_bytes == NULL)
        return;

    rcsa_crc32_t crc;
    rcsa_crc32_init(&crc);
    rcsa_crc32_update(&crc, header_bytes, UDPPACKETHEADER_SIZE);
    rcsa_crc32_update(&crc, packet->payload, packet->header->payload_size);
    uint32_t crc_final = rcsa_crc32_get(&crc);

    uint8_t crc_buf[sizeof(uint32_t)];
    memcpy(crc_buf, &crc_final, sizeof(uint32_t));


    SendBytesSerialComm(serial, header_bytes, UDPPACKETHEADER_SIZE);
    if (packet->header->payload_size > 0)
        SendBytesSerialComm(serial, packet->payload, packet->header->payload_size);

    SendBytesSerialComm(serial, crc_buf, sizeof(uint32_t));

    free(header_bytes);
    header_bytes = NULL;
}
