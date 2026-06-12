#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "driver/Packet_RCSA/common_defs.h"
#include "app_state.h"
#include "rtsp_streamer.hpp"

#include <cstdio>
#include <cstring>

#define BUF_SIZE SPI_COMM_BUF_SIZE

// เฟรมภาพ QVGA grayscale 320x240 = 76800 ไบต์ = SPI_COMM_DATA_PAYLOAD_SIZE
#define IMAGE_WIDTH  320
#define IMAGE_HEIGHT 240

static hal::SPIBus*    g_spi   = nullptr;
static hal::GPIOReady* g_ready = nullptr;

static uint8_t s_tx_buf[BUF_SIZE];
static uint8_t s_rx_buf[BUF_SIZE];

void app_init() {
    spi_comm_init();
    app_state_init(SPI_COMM_DATA_CAPACITY);
    g_spi   = new hal::SPIBus("/dev/spidev0.0", 10000000);
    g_ready = new hal::GPIOReady("/dev/gpiochip4", 22);
    if (!rtsp_streamer_start(IMAGE_WIDTH, IMAGE_HEIGHT, 15, "/spi")) {
        std::fprintf(stderr, "[RTSP] disabled because server failed to start\n");
    }
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

    if (pkt) FreeUDPPacket(pkt);

    if (data_ready) {
        uint32_t data_size = 0;
        const uint8_t* data = app_state_get_ready_data(&data_size);

        // Publish the latest complete SPI frame to the RTSP encoder.
        if (data_size >= (uint32_t)(IMAGE_WIDTH * IMAGE_HEIGHT)) {
            rtsp_streamer_publish_gray8(data, data_size);
        }
    }
}
