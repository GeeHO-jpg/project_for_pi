#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "driver/Packet_RCSA/common_defs.h"
#include "app_state.h"
#include "rtsp_streamer.hpp"

#include <chrono>
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

namespace {

using Clock = std::chrono::steady_clock;

struct Stats {
    Clock::time_point window_start = Clock::now();
    uint64_t ticks = 0;
    uint64_t frames = 0;
    uint64_t wait_ns_total = 0;
    uint64_t wait_ns_max = 0;
    uint64_t spi_ns_total = 0;
    uint64_t spi_ns_max = 0;
};

Stats g_stats;

uint64_t elapsed_ns(Clock::time_point start, Clock::time_point end) {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

double ns_to_ms(uint64_t ns) {
    return (double)ns / 1000000.0;
}

void stats_add_duration(uint64_t ns, uint64_t& total, uint64_t& max_value) {
    total += ns;
    if (ns > max_value) {
        max_value = ns;
    }
}

void stats_maybe_print() {
    auto now = Clock::now();
    uint64_t window_ns = elapsed_ns(g_stats.window_start, now);
    if (window_ns < 1000000000ull) {
        return;
    }

    double seconds = (double)window_ns / 1000000000.0;
    double ticks_per_sec = (double)g_stats.ticks / seconds;
    double frames_per_sec = (double)g_stats.frames / seconds;
    double wait_avg_ms = g_stats.ticks ? ns_to_ms(g_stats.wait_ns_total / g_stats.ticks) : 0.0;
    double spi_avg_ms = g_stats.ticks ? ns_to_ms(g_stats.spi_ns_total / g_stats.ticks) : 0.0;

    AppStateDebugCounters state_debug{};
    app_state_get_debug_counters(&state_debug);

    std::printf("[STATS] ticks=%.1f/s frames=%.1f/s wait_avg=%.3fms wait_max=%.3fms "
                "spi_avg=%.3fms spi_max=%.3fms rtsp_fail=%llu resync=%u "
                "tx_info=%u tx_data=%u tx_frame_start=%u "
                "no_pkt=%u wrong_cmd=%u bad_payload=%u wrong_index=%u incomplete=%u commits=%u "
                "last_idx=%u/%u last_payload=%u\n",
                ticks_per_sec,
                frames_per_sec,
                wait_avg_ms,
                ns_to_ms(g_stats.wait_ns_max),
                spi_avg_ms,
                ns_to_ms(g_stats.spi_ns_max),
                (unsigned long long)rtsp_streamer_get_push_fail_count(),
                app_state_get_resync_count(),
                state_debug.tx_info,
                state_debug.tx_data,
                state_debug.tx_frame_start,
                state_debug.no_pkt,
                state_debug.wrong_cmd,
                state_debug.bad_payload,
                state_debug.wrong_index,
                state_debug.incomplete_frames,
                state_debug.commits,
                state_debug.last_expected_index,
                state_debug.last_got_index,
                state_debug.last_payload_size);

    g_stats = Stats{};
}

} // namespace

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
    g_stats.ticks++;

    // ── PREPARE: เตรียม tx packet ตัวถัดไปตาม state ปัจจุบัน ────────────────
    app_state_prepare_tx();

    // ── Driver: drain TX ring buffer → flat buffer ────────────────────────
    memset(s_tx_buf, 0, BUF_SIZE);
    spi_comm_drain_tx(s_tx_buf, BUF_SIZE);

    // ── HAL: wait for ESP32 ready signal ──────────────────────────────────
    auto wait_start = Clock::now();
    g_ready->waitReady();
    auto wait_end = Clock::now();
    stats_add_duration(elapsed_ns(wait_start, wait_end), g_stats.wait_ns_total, g_stats.wait_ns_max);

    // ── TX: SPI full-duplex transfer (rx ที่ได้คือคำตอบของ tx รอบก่อนหน้า) ──
    auto spi_start = Clock::now();
    g_spi->transfer(s_tx_buf, s_rx_buf, BUF_SIZE);
    auto spi_end = Clock::now();
    stats_add_duration(elapsed_ns(spi_start, spi_end), g_stats.spi_ns_total, g_stats.spi_ns_max);

    // ── Driver: push RX bytes → RX ring buffer → parse ────────────────────
    spi_comm_push_rx(s_rx_buf, BUF_SIZE);
    UDPPacket* pkt = spi_comm_parse_rx();

    // ── PARSE: ป้อนผลลัพธ์เข้า state machine ───────────────────────────────
    bool data_ready = app_state_handle_rx(pkt);

    if (pkt) FreeUDPPacket(pkt);

    if (data_ready) {
        g_stats.frames++;
        uint32_t data_size = 0;
        const uint8_t* data = app_state_get_ready_data(&data_size);

        // Publish the latest complete SPI frame to the RTSP encoder.
        if (data_size >= (uint32_t)(IMAGE_WIDTH * IMAGE_HEIGHT)) {
            rtsp_streamer_publish_gray8(data, data_size);
        }
    }

    stats_maybe_print();
}
