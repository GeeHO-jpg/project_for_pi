#include "app_spi.h"
#include "hal/spi_bus.hpp"
#include "hal/gpio_ready.hpp"
#include "driver/spi_comm.h"
#include "protocol/spi_cmd.h"
#include "protocol/frame_assembler.h"

#include <cstdio>
#include <cstring>

/* ── Configuration ───────────────────────────────────────────────────────── */
static constexpr int READY_TIMEOUT_MS = 1000;
static constexpr int MAX_RETRY        = 3;

/* ── HAL handles ─────────────────────────────────────────────────────────── */
static hal::SPIBus*    g_spi   = nullptr;
static hal::GPIOReady* g_ready = nullptr;

/* ── Buffers ─────────────────────────────────────────────────────────────── */
static uint8_t s_tx_buf [SPI_COMM_BUF_SIZE];
static uint8_t s_rx_buf [SPI_COMM_BUF_SIZE];
static uint8_t s_payload[SPI_COMM_PAYLOAD_SIZE];

/* ── State machine ───────────────────────────────────────────────────────── */
typedef enum { STATE_INFO, STATE_CHUNK } AppState;

static AppState  s_state       = STATE_INFO;
static uint16_t  s_chunk_index = 0;
static int       s_retry       = 0;

/* ── Frame callback ──────────────────────────────────────────────────────── */
static void on_frame_complete(const uint8_t *buf, uint32_t size)
{
    printf("[FRAME] complete — %u bytes\n", (unsigned)size);
    (void)buf;
    /* TODO: hand buf off to optical-flow or other consumer before reset */
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Execute one SPI transaction.
   Builds packet, waits GPIO ready, runs full-duplex transfer, pushes RX bytes.
   Returns false on ready timeout (RX ring buffer is NOT populated). */
static bool do_transfer(uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    spi_comm_build_tx(cmd, payload, payload_len);

    memset(s_tx_buf, 0, SPI_COMM_BUF_SIZE);
    spi_comm_drain_tx(s_tx_buf, SPI_COMM_BUF_SIZE);

    if (!g_ready->waitReady(READY_TIMEOUT_MS)) {
        printf("[SPI] ready timeout (cmd=0x%02X)\n", cmd);
        return false;
    }

    g_spi->transfer(s_tx_buf, s_rx_buf, SPI_COMM_BUF_SIZE);
    spi_comm_push_rx(s_rx_buf, SPI_COMM_BUF_SIZE);
    return true;
}

/* Reset everything and return to INFO stage. */
static void go_info()
{
    frame_asm_reset();
    s_state       = STATE_INFO;
    s_chunk_index = 0;
    s_retry       = 0;
}

/* ── App ─────────────────────────────────────────────────────────────────── */

void app_init()
{
    spi_comm_init();
    g_spi   = new hal::SPIBus("/dev/spidev0.0", 500000);
    g_ready = new hal::GPIOReady("/dev/gpiochip4", 22);
}

void app_tick()
{
    /* ════════════════════════════════════════════════════════════════════════
     *  STATE_INFO — request image metadata from slave
     * ════════════════════════════════════════════════════════════════════════ */
    if (s_state == STATE_INFO) {
        uint16_t plen = spi_cmd_pack_info_req(s_payload);

        if (!do_transfer(SPI_CMD_CAM_INFO, s_payload, plen))
            return;  /* timeout — retry next tick */

        UDPPacket *pkt = spi_comm_parse_rx();
        if (!pkt) {
            printf("[INFO] bad packet (CRC/signature)\n");
            return;
        }

        SpiCamInfo info;
        bool ok = spi_cmd_unpack_info_resp(pkt->payload, pkt->header->payload_size, &info);
        FreeUDPPacket(pkt);

        if (!ok) {
            printf("[INFO] unpack failed\n");
            return;
        }

        printf("[INFO] %ux%u  chunks=%u  chunk_size=%u\n",
               info.width, info.height, info.chunk_count, info.chunk_size);

        frame_asm_init(info.width, info.height,
                       info.chunk_count, info.chunk_size,
                       on_frame_complete);

        s_state       = STATE_CHUNK;
        s_chunk_index = 0;
        s_retry       = 0;
        return;
    }

    /* ════════════════════════════════════════════════════════════════════════
     *  STATE_CHUNK — request one chunk at a time, assemble into frame
     * ════════════════════════════════════════════════════════════════════════ */
    uint16_t plen = spi_cmd_pack_chunk_req(s_payload, s_chunk_index);

    if (!do_transfer(SPI_CMD_CAM_CHUNK, s_payload, plen)) {
        printf("[CHUNK] ready timeout → restart INFO\n");
        go_info();
        return;
    }

    UDPPacket *pkt = spi_comm_parse_rx();
    if (!pkt) {
        if (++s_retry > MAX_RETRY) {
            printf("[CHUNK] max retry exceeded → restart INFO\n");
            go_info();
        } else {
            printf("[CHUNK] CRC fail (retry %d/%d)\n", s_retry, MAX_RETRY);
        }
        return;
    }

    /* Extract before freeing the packet */
    uint16_t psize    = pkt->header->payload_size;
    uint16_t data_len = (psize > (uint16_t)sizeof(uint16_t))
                      ? (uint16_t)(psize - sizeof(uint16_t))
                      : 0u;

    SpiChunkResp resp;
    bool ok = spi_cmd_unpack_chunk_resp(pkt->payload, psize, &resp);
    FreeUDPPacket(pkt);

    if (!ok || resp.chunk_index != s_chunk_index) {
        if (++s_retry > MAX_RETRY) {
            printf("[CHUNK] max retry exceeded → restart INFO\n");
            go_info();
        } else {
            printf("[CHUNK] unexpected resp (retry %d/%d)\n", s_retry, MAX_RETRY);
        }
        return;
    }

    s_retry = 0;

    bool complete = frame_asm_add_chunk(resp.chunk_index, resp.data, data_len);
    if (complete) {
        go_info();   /* frame_asm_reset() is inside go_info() */
    } else {
        s_chunk_index++;
    }
}
