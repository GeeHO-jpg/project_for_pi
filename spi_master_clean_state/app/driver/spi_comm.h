#pragma once

#include <stdint.h>
#include "Packet_RCSA/UDPPacket.h"

// Frame sizes
#define SPI_COMM_DATA_PAYLOAD_SIZE   19u  // CMD_DATA: ขนาดข้อมูลทั้งหมด (k_payload)
#define SPI_COMM_DATA_CHUNK_SIZE     4u   // CMD_DATA: ขนาดข้อมูลต่อ 1 chunk

// CMD_DATA: จำนวน chunk ทั้งหมด และขนาด buffer รวม (chunk สุดท้ายอาจมีข้อมูลไม่เต็ม แต่ส่งเต็ม chunk เสมอ)
#define SPI_COMM_DATA_TOTAL_CHUNKS  ((SPI_COMM_DATA_PAYLOAD_SIZE + SPI_COMM_DATA_CHUNK_SIZE - 1u) / SPI_COMM_DATA_CHUNK_SIZE)  // 5
#define SPI_COMM_DATA_CAPACITY      (SPI_COMM_DATA_TOTAL_CHUNKS * SPI_COMM_DATA_CHUNK_SIZE)  // 20

// payload ต่อ 1 SPI frame: 1 byte (chunk_index) + ข้อมูล (สำหรับ CMD_INFO ใช้ 2 byte แรกเป็น chunk_size/total_chunks)
#define SPI_COMM_FRAME_PAYLOAD_SIZE  (1u + SPI_COMM_DATA_CHUNK_SIZE)  // 5
#define SPI_COMM_BUF_SIZE       (UDPPACKETHEADER_SIZE + SPI_COMM_FRAME_PAYLOAD_SIZE + 4u)  // 18

#ifdef __cplusplus
extern "C" {
#endif

void spi_comm_init(void);

// TX: build RCSA packet → push bytes to TX ring buffer
void spi_comm_build_tx(uint8_t cmd, const uint8_t *payload, uint16_t len);

// TX: drain TX ring buffer → flat buffer (returns bytes written)
uint16_t spi_comm_drain_tx(uint8_t *buf, uint16_t buf_size);

// RX: push received bytes → RX ring buffer
void spi_comm_push_rx(const uint8_t *buf, uint16_t len);

// RX: parse from RX ring buffer → return completed packet (caller must FreeUDPPacket)
UDPPacket* spi_comm_parse_rx(void);

#ifdef __cplusplus
}
#endif
