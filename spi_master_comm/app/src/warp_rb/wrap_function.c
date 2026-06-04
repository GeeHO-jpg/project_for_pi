/*
 * app_ton.c
 *
 * New role:
 * - send RCSA packets directly into i2c_tx_rb
 * - keep old API names so upper layer code does not need large changes
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>


#include "ring_buffer.h"
//#include "i2c_driver.h"
#include "../Packet_RCSA/UDPPacketHeader.h"
#include "../Packet_RCSA/SerialComm.h"
#include "../Packet_RCSA/crc_32.h"
#include "warp_function.h"



/* ถ้า byte 8,9 ที่คุณพูดถึงเป็นการนับแบบ 1-based
 * offset จาก magic จะเป็น 7 และ 8
 */
#define UDP_LEN_H_OFFSET 7U
#define UDP_LEN_L_OFFSET 8U
extern RingBuffer i2c_tx_slave_rb;
extern RingBuffer i2c_rx_slave_rb;
extern RingBuffer i2c_tx_master_rb;
extern RingBuffer i2c_rx_master_rb;

SerialComm* serial_i2c = NULL;
//static bool s_i2c_comm_initialized = false;

static uint16_t MasterI2CRead(uint8_t* buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0U))
    {
        return 0U;
    }

    return rb_get(&i2c_rx_master_rb, buffer, length);
}

void I2CMasterWriteByte(uint8_t byte)
{
    (void)rb_put_byte(&i2c_tx_master_rb, byte);
}

bool I2CMasterReadByte(uint8_t* byte)
{
    return (MasterI2CRead(byte, 1U) == 1U);
}


static uint16_t SlaveI2CRead(uint8_t* buffer, uint16_t length)
{
    if ((buffer == NULL) || (length == 0U))
    {
        return 0U;
    }

    return rb_get(&i2c_rx_slave_rb, buffer, length);
}

void I2CSlaveWriteByte(uint8_t byte)
{
    (void)rb_put_byte(&i2c_tx_slave_rb, byte);
}

bool I2CSlaveReadByte(uint8_t* byte)
{
    return (SlaveI2CRead(byte, 1U) == 1U);
}





uint16_t rb_peek_bytes(RingBuffer *rb, uint8_t *dst, uint16_t len)
{
    uint16_t count = 0U;

    if (rb == NULL || rb->buffer == NULL || dst == NULL || len == 0U || rb->size < 2U)
    {
        return 0U;
    }

    unsigned long primask = __get_PRIMASK();
    __disable_irq();

    uint16_t head = rb->head;
    uint16_t tail = rb->tail;

    while ((tail != head) && (count < len))
    {
        dst[count] = rb->buffer[tail];
        tail = (uint16_t)((tail + 1U) % rb->size);
        count++;
    }

    __set_PRIMASK(primask);
    return count;
}

static void rb_discard_bytes(RingBuffer *rb, uint16_t len)
{
    uint8_t sink[16];

    while (len > 0U)
    {
        uint16_t chunk = (len > (uint16_t)sizeof(sink)) ? (uint16_t)sizeof(sink) : len;
        uint16_t got = rb_get(rb, sink, chunk);

        if (got == 0U)
        {
            break;
        }

        len = (uint16_t)(len - got);
    }
}

bool i2c_extract_one_packet(uint8_t *tx_buf, uint16_t tx_buf_size, uint16_t *out_len)
{
    uint8_t header[UDPPACKETHEADER_SIZE];
    uint16_t available;
    uint16_t payload_size;
    uint16_t total_size;

    if (tx_buf == NULL || out_len == NULL || tx_buf_size == 0U)
    {
        return false;
    }

    *out_len = 0U;
    available = rb_get_count(&i2c_tx_slave_rb);
    if (available < UDPPACKETHEADER_SIZE)
    {
        return false;
    }

    if (rb_peek_bytes(&i2c_tx_slave_rb, header, UDPPACKETHEADER_SIZE) != UDPPACKETHEADER_SIZE)
    {
        return false;
    }

    if (!IsPacketHeaderSignature(header))
    {
        rb_discard_bytes(&i2c_tx_slave_rb, 1U);

        return false;
    }

    payload_size = (uint16_t)header[7] | ((uint16_t)header[8] << 8);
    total_size = (uint16_t)(UDPPACKETHEADER_SIZE + payload_size + 4U);

    if (total_size > tx_buf_size)
    {
        if (available >= total_size)
        {
            rb_discard_bytes(&i2c_tx_slave_rb, total_size);


        }
        return false;
    }

    if (available < total_size)
    {
        return false;
    }

    if (rb_get(&i2c_tx_slave_rb, tx_buf, total_size) != total_size)
    {
        return false;
    }

    *out_len = total_size;
    return true;
}

uint16_t i2c_peek_one_packet(uint16_t tx_buf_size)
{
    uint8_t header[UDPPACKETHEADER_SIZE];
    uint16_t available;
    uint16_t payload_size;
    uint16_t total_size;

    if (tx_buf_size == 0U)
    {
        return 0U;
    }

    available = rb_get_count(&i2c_tx_slave_rb);
    if (available < UDPPACKETHEADER_SIZE)
    {
        return 0U;
    }

    if (rb_peek_bytes(&i2c_tx_slave_rb, header, UDPPACKETHEADER_SIZE) != UDPPACKETHEADER_SIZE)
    {
        return 0U;
    }

    if (!IsPacketHeaderSignature(header))
    {
        rb_discard_bytes(&i2c_tx_slave_rb, 1U);
        return 0U;
    }

    payload_size = (uint16_t)header[7] | ((uint16_t)header[8] << 8);
    total_size = (uint16_t)(UDPPACKETHEADER_SIZE + payload_size + 4U);

    if (total_size > tx_buf_size)
    {
        return 0U;
    }

    return total_size;
}

static bool IsInitializeI2CComm()
{
    return IsOperableSerialComm(serial_i2c);
}

void InitializeI2CComm(UARTReadFunction r_func,
                       UARTWriteFunction w_func)
{
    if (IsInitializeI2CComm())
        return;

    serial_i2c = CreateSerialComm(r_func, w_func);
}

void RunReceiveI2CComm()
{
    RunReceiveSerialComm(serial_i2c);
}

UDPPacket* GetReceivedUDPPacketI2CComm()
{
    return GetCompletePacketSerialComm(serial_i2c);
}


//static bool QueueBytesToI2CTx(const uint8_t* data, uint16_t length)
//{
//    if ((data == NULL) || (length == 0U))
//    {
//        return false;
//    }
//
//    return rb_put_exact(&i2c_tx_slave_rb, data, length);
//}


void SendUDPPacketI2CComm(UDPPacket* packet)
{
	SendUDPPacketSerialComm(serial_i2c, packet);
}
