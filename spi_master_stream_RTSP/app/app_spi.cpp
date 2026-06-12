#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "driver/Packet_RCSA/common_defs.h"
#include "app_state.h"

#include <cstdio>
#include <cstring>

#define BUF_SIZE SPI_COMM_BUF_SIZE

// เฟรมภาพ QVGA grayscale 320x240 = 76800 ไบต์ = SPI_COMM_DATA_PAYLOAD_SIZE
#define IMAGE_WIDTH  320
#define IMAGE_HEIGHT 240
#define OUTPUT_MODE_RTSP 1
// ── เลือกโหมด output ตอน build ─────────────────────────────────────────────
// ปกติ (ไม่ define อะไร)  -> แสดงผลผ่านหน้าต่าง OpenCV (imshow)
// -DOUTPUT_MODE_RTSP      -> เปิด ffmpeg เป็น subprocess แล้ว push H.264/RTSP ไปยัง mediamtx
#if defined(OUTPUT_MODE_RTSP)
  #include <csignal>

  #define STREAM_FPS 30

  #ifndef RTSP_URL
  #define RTSP_URL "rtsp://127.0.0.1:8554/spi"
  #endif

  static FILE* g_ffmpeg = nullptr;
#else
  #include <opencv2/opencv.hpp>

  static const char* WINDOW_NAME = "SPI Frame";
#endif

static hal::SPIBus*    g_spi   = nullptr;
static hal::GPIOReady* g_ready = nullptr;

static uint8_t s_tx_buf[BUF_SIZE];
static uint8_t s_rx_buf[BUF_SIZE];

void app_init() {
    spi_comm_init();
    app_state_init(SPI_COMM_DATA_CAPACITY);
    g_spi   = new hal::SPIBus("/dev/spidev0.0", 1000000);
    g_ready = new hal::GPIOReady("/dev/gpiochip4", 22);

#if defined(OUTPUT_MODE_RTSP)
    // กัน process ตายด้วย SIGPIPE ถ้า ffmpeg ปลายทาง pipe หลุด/ตาย
    signal(SIGPIPE, SIG_IGN);

    // เปิด ffmpeg รับ raw grayscale frame ทาง stdin -> encode H.264 -> push RTSP ไปยัง mediamtx
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -loglevel warning -f rawvideo -pixel_format gray "
        "-video_size %dx%d -framerate %d -i - "
        "-c:v libx264 -preset ultrafast -tune zerolatency -pix_fmt yuv420p "
        "-f rtsp -rtsp_transport tcp %s",
        IMAGE_WIDTH, IMAGE_HEIGHT, STREAM_FPS, RTSP_URL);

    g_ffmpeg = popen(cmd, "w");
    if (!g_ffmpeg) {
        fprintf(stderr, "[RTSP] failed to start ffmpeg: %s\n", cmd);
    }
#else
    cv::namedWindow(WINDOW_NAME, cv::WINDOW_AUTOSIZE);
#endif
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

        if (data_size >= (uint32_t)(IMAGE_WIDTH * IMAGE_HEIGHT)) {
#if defined(OUTPUT_MODE_RTSP)
            // ส่งเฟรม raw grayscale เข้า ffmpeg เพื่อ encode + push RTSP
            if (g_ffmpeg) {
                fwrite(data, 1, (size_t)IMAGE_WIDTH * IMAGE_HEIGHT, g_ffmpeg);
            }
#else
            // แสดงเฟรมล่าสุดแบบเรียลไทม์ผ่านหน้าต่าง OpenCV
            cv::Mat img(IMAGE_HEIGHT, IMAGE_WIDTH, CV_8UC1, (void*)data);
            cv::imshow(WINDOW_NAME, img);
            cv::waitKey(1);
#endif
        }
    }
}
