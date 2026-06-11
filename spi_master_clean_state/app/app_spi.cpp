#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "driver/Packet_RCSA/common_defs.h"
#include "app_state.h"

#include <cstdio>
#include <cstring>

#define BUF_SIZE SPI_COMM_BUF_SIZE

static hal::SPIBus*    g_spi   = nullptr;
static hal::GPIOReady* g_ready = nullptr;

static uint8_t s_tx_buf[BUF_SIZE];
static uint8_t s_rx_buf[BUF_SIZE];

void app_init() {
    spi_comm_init();
    app_state_init(SPI_COMM_DATA_CAPACITY);
    g_spi   = new hal::SPIBus("/dev/spidev0.0", 1000000);
    g_ready = new hal::GPIOReady("/dev/gpiochip4", 22);
}

void app_tick() {

    // ── PREPARE: เตรียม tx packet ตัวถัดไปตาม state ปัจจุบัน ────────────────
    app_state_prepare_tx();

    // ── Driver: drain TX ring buffer → flat buffer ────────────────────────
    memset(s_tx_buf, 0, BUF_SIZE);
    spi_comm_drain_tx(s_tx_buf, BUF_SIZE);

    // ── HAL: wait for ESP32 ready signal ──────────────────────────────────
    g_ready->waitReady();

    // ── TX: SPI full-duplex transfer (rx ที่ได้คือคำตอบของ tx รอบก่อนหน้า) ──
    g_spi->transfer(s_tx_buf, s_rx_buf, BUF_SIZE);

    // ── Driver: push RX bytes → RX ring buffer → parse ────────────────────
    spi_comm_push_rx(s_rx_buf, BUF_SIZE);
    UDPPacket* pkt = spi_comm_parse_rx();

    // ── PARSE: ป้อนผลลัพธ์เข้า state machine ───────────────────────────────
    bool data_ready = app_state_handle_rx(pkt);

    // ── Print ─────────────────────────────────────────────────────────────
    printf("[TX]:");
    for (size_t i = 0; i < BUF_SIZE; i++) printf(" %02X", s_tx_buf[i]);
    printf("\n[RX]:");
    for (size_t i = 0; i < BUF_SIZE; i++) printf(" %02X", s_rx_buf[i]);

    if (pkt) {
        printf("\n[RX] OK  id=%u cmd=0x%02X\n", pkt->header->id, pkt->header->cmd);
        FreeUDPPacket(pkt);
    } else {
        printf("\n[RX] FAIL (bad signature or CRC)\n");
    }

    if (data_ready) {
        uint8_t chunk_size = 0, total_chunks = 0;
        app_state_get_info(&chunk_size, &total_chunks);
        printf("[INFO] chunk_size=%u total_chunks=%u\n", chunk_size, total_chunks);

        uint16_t data_size = 0;
        const uint8_t* data = app_state_get_ready_data(&data_size);
        printf("[DATA] ready, size=%u:", data_size);
        for (uint16_t i = 0; i < data_size; i++) printf(" %02X", data[i]);
        printf("\n");
    }
    printf("\n");
}
