#pragma once

#include <stdint.h>
#include "Packet_RCSA/UDPPacket.h"

// Frame sizes
#define SPI_COMM_DATA_PAYLOAD_SIZE   76800u  // CMD_DATA: ขนาดข้อมูลทั้งหมด (เฟรมภาพ QVGA grayscale 320x240)
#define SPI_COMM_DATA_CHUNK_SIZE     3274u   // CMD_DATA: ขนาดข้อมูลต่อ 1 chunk (~80% ของเพดาน SPI/DMA 4092 ไบต์)

// CMD_DATA: จำนวน chunk ทั้งหมด และขนาด buffer รวม (chunk สุดท้ายมีข้อมูลไม่เต็ม chunk_size)
#define SPI_COMM_DATA_TOTAL_CHUNKS  ((SPI_COMM_DATA_PAYLOAD_SIZE + SPI_COMM_DATA_CHUNK_SIZE - 1u) / SPI_COMM_DATA_CHUNK_SIZE)  // 24
#define SPI_COMM_DATA_CAPACITY      (SPI_COMM_DATA_TOTAL_CHUNKS * SPI_COMM_DATA_CHUNK_SIZE)  // 78576

// payload ต่อ 1 SPI frame: 2 byte (chunk_index, uint16_t little-endian) + ข้อมูล (chunk_size ไบต์)
// CMD_INFO ใช้ payload[0:1]=chunk_size, payload[2:3]=total_chunks, payload[4:5]=last_chunk_size (ทั้งหมด uint16_t little-endian)
#define SPI_COMM_FRAME_PAYLOAD_SIZE  (2u + SPI_COMM_DATA_CHUNK_SIZE)  // 3276
#define SPI_COMM_BUF_SIZE       (UDPPACKETHEADER_SIZE + SPI_COMM_FRAME_PAYLOAD_SIZE + 4u)  // 3289

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

// RX: ล้าง rx ring buffer + state การ parse ที่ค้างอยู่ (ใช้ตอน resync หลัง stall ยาวนาน)
void spi_comm_flush_rx(void);

#ifdef __cplusplus
}
#endif
