#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "driver/Packet_RCSA/common_defs.h"

#include <cstdio>
#include <cstring>

#include "chunk_manage.h"

#define CHUNK_SIZE 2
static hal::SPIBus*    g_spi   = nullptr;
static hal::GPIOReady* g_ready = nullptr;

static uint8_t s_tx_buf[9 + CHUNK_SIZE + 4];
static uint8_t s_rx_buf[9 + CHUNK_SIZE + 4];



static const uint8_t k_payload[SPI_COMM_PAYLOAD_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13
};


void app_init() {
    spi_comm_init();
    g_spi   = new hal::SPIBus("/dev/spidev0.0", 1000000);
    g_ready = new hal::GPIOReady("/dev/gpiochip4", 22);
}

void app_tick() {

    static uint16_t chunk_total =0;
    static uint16_t chunk_index = 0;
    static uint8_t chunk_buf[(9+CHUNK_SIZE+4)];

    chunk_total = total_chunks(SPI_COMM_PAYLOAD_SIZE, CHUNK_SIZE);
    for (uint16_t i = 0; i < chunk_total; i++)
    {
        // เลือก chunk ถัดไป (จะวนกลับไป chunk แรกเมื่อครบ)
        uint16_t chunk_len = select_chunk(chunk_buf, k_payload, SPI_COMM_PAYLOAD_SIZE, CHUNK_SIZE);

        // ── Driver: build TX packet → TX ring buffer ──────────────────────────
        spi_comm_build_tx(CMD_HANDSHAKE, chunk_buf, chunk_len);

        // ── Driver: drain TX ring buffer → flat buffer ────────────────────────
        memset(s_tx_buf, 0, (9+CHUNK_SIZE+4)); // Clear only the part we will use
        spi_comm_drain_tx(s_tx_buf, (9+CHUNK_SIZE+4));

        // ── HAL: wait for ESP32 ready signal ──────────────────────────────────
        g_ready->waitReady();

        // ── HAL: SPI full-duplex transfer ─────────────────────────────────────
        g_spi->transfer(s_tx_buf, s_rx_buf, (9+CHUNK_SIZE+4));

        // ── Driver: push RX bytes → RX ring buffer → parse ────────────────────
        spi_comm_push_rx(s_rx_buf, (9+CHUNK_SIZE+4));
        UDPPacket* pkt = spi_comm_parse_rx();

        // ── Print ─────────────────────────────────────────────────────────────
        printf("[TX]:");
        for (size_t i = 0; i < (9+CHUNK_SIZE+4); i++) printf(" %02X", s_tx_buf[i]);
        printf("\n[RX]:");
        for (size_t i = 0; i < (9+CHUNK_SIZE+4); i++) printf(" %02X", s_rx_buf[i]);

        if (pkt) {
            printf("\n[RX] OK  id=%u cmd=0x%02X\n\n", pkt->header->id, pkt->header->cmd);
            FreeUDPPacket(pkt);
        } else {
            printf("\n[RX] FAIL (bad signature or CRC)\n\n");
        }
    }

}
