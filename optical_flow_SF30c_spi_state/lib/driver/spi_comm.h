#pragma once

#include <stdint.h>
#include "protocal/Packet_RCSA/UDPPacket.h"

// Frame sizes
#define SPI_COMM_PAYLOAD_SIZE   19u
#define SPI_COMM_BUF_SIZE       (UDPPACKETHEADER_SIZE + SPI_COMM_PAYLOAD_SIZE + 4u)  // 32

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

// RX: discard all bytes currently in the RX ring buffer (call after transfer timeout)
void spi_comm_flush_rx(void);

#ifdef __cplusplus
}
#endif
