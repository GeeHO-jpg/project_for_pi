#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "driver/Packet_RCSA/common_defs.h"
#include "app_state.h"

#include <cstdio>
#include <cstring>
#include <opencv2/opencv.hpp>

#define BUF_SIZE SPI_COMM_BUF_SIZE

// เฟรมภาพ QVGA grayscale 320x240 = 76800 ไบต์ = SPI_COMM_DATA_PAYLOAD_SIZE
#define IMAGE_WIDTH  320
#define IMAGE_HEIGHT 240

static const char* WINDOW_NAME = "SPI Frame";

static hal::SPIBus*    g_spi   = nullptr;
static hal::GPIOReady* g_ready = nullptr;

static uint8_t s_tx_buf[BUF_SIZE];
static uint8_t s_rx_buf[BUF_SIZE];

// ── little-endian uint16 helper (wire format, ใช้สำหรับ debug print) ──
static inline uint16_t get_u16le(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void app_init() {
    spi_comm_init();
    app_state_init(SPI_COMM_DATA_CAPACITY);
    g_spi   = new hal::SPIBus("/dev/spidev0.0", 1000000);
    g_ready = new hal::GPIOReady("/dev/gpiochip4", 22);
    cv::namedWindow(WINDOW_NAME, cv::WINDOW_AUTOSIZE);
}

void app_tick() {

    // เก็บ state/chunk_index ที่ "กำลังจะขอ" ไว้ก่อน prepare_tx จะเปลี่ยนค่า (ใช้ debug print)
    CommState state_before     = app_state_get();
    uint16_t  requested_index  = (state_before == CommState::DataRequest) ? app_state_get_next_index() : 0;

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

    // ── Print: 1 บรรทัด/tick สรุป TX cmd/idx -> RX cmd/idx/payload ───────────
    if (state_before == CommState::InfoRequest) {
        printf("[INFO_REQUEST] TX cmd=INFO          -> ");
    } else {
        printf("[DATA_REQUEST] TX cmd=DATA idx=%-5u -> ", requested_index);
    }

    if (pkt) {
        if (pkt->header->cmd == static_cast<uint8_t>(CommCmd::Info)) {
            uint16_t chunk_size = 0, total_chunks = 0, last_chunk_size = 0;
            if (pkt->header->payload_size >= 6) {
                chunk_size      = get_u16le(&pkt->payload[0]);
                total_chunks    = get_u16le(&pkt->payload[2]);
                last_chunk_size = get_u16le(&pkt->payload[4]);
            }
            printf("RX cmd=INFO payload=%u (chunk_size=%u total_chunks=%u last_chunk_size=%u)\n",
                   pkt->header->payload_size, chunk_size, total_chunks, last_chunk_size);
        } else if (pkt->header->cmd == static_cast<uint8_t>(CommCmd::Data)) {
            uint16_t echoed_index = (pkt->header->payload_size >= 2) ? get_u16le(&pkt->payload[0]) : 0;
            const char* note = "";
            if (state_before == CommState::DataRequest && echoed_index != requested_index) {
                note = " (stale, retry)";
            }
            printf("RX cmd=DATA idx=%u payload=%u%s\n", echoed_index, pkt->header->payload_size, note);
        } else {
            printf("RX cmd=0x%02X payload=%u (unexpected)\n", pkt->header->cmd, pkt->header->payload_size);
        }
        FreeUDPPacket(pkt);
    } else {
        printf("RX FAIL (bad signature or CRC) -- retry\n");
    }

    if (data_ready) {
        uint16_t chunk_size = 0, total_chunks = 0;
        app_state_get_info(&chunk_size, &total_chunks);

        uint32_t data_size = 0;
        const uint8_t* data = app_state_get_ready_data(&data_size);
        printf("[FRAME READY] size=%u bytes (chunks=%u, chunk_size=%u), first:", data_size, total_chunks, chunk_size);
        for (uint32_t i = 0; i < 8 && i < data_size; i++) printf(" %02X", data[i]);
        printf("\n");

        // แสดงเฟรมล่าสุดแบบเรียลไทม์ผ่านหน้าต่าง OpenCV
        if (data_size >= (uint32_t)(IMAGE_WIDTH * IMAGE_HEIGHT)) {
            cv::Mat img(IMAGE_HEIGHT, IMAGE_WIDTH, CV_8UC1, (void*)data);
            cv::imshow(WINDOW_NAME, img);
            cv::waitKey(1);
        }
    }
}
