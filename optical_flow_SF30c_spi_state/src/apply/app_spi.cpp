#include "app_spi.h"
#include "spi_comm.h"
#include "spi_cmd_slave.h"
#include "../config/param_conf.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <cstring>
#include <cstdio>

#include "driver/spi_slave.h"
#include "driver/gpio.h"

// ── SPI Slave HAL ─────────────────────────────────────────────────────────────

// เรียกจาก ISR ทันทีหลัง SPI hardware โหลด TX buffer เสร็จ → ไม่มี race condition
static void IRAM_ATTR spi_post_setup_cb(spi_slave_transaction_t *trans) {
    (void)trans;
    gpio_set_level((gpio_num_t)PIN_DATA_READY, 1);
}

static void spi_hal_init() {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num   = PIN_MOSI;
    bus_cfg.miso_io_num   = PIN_MISO;
    bus_cfg.sclk_io_num   = PIN_SCK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;

    spi_slave_interface_config_t slv_cfg = {};
    slv_cfg.spics_io_num  = PIN_CS;
    slv_cfg.flags         = 0;
    slv_cfg.queue_size    = 1;
    slv_cfg.mode          = 0;
    slv_cfg.post_setup_cb = spi_post_setup_cb;  // raise DATA_READY เมื่อ HW พร้อม
    slv_cfg.post_trans_cb = NULL;

    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &bus_cfg, &slv_cfg, SPI_DMA_CH_AUTO));
}

static void gpio_hal_init() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask  = (1ULL << PIN_DATA_READY);
    io_conf.mode          = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en    = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en  = GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)PIN_DATA_READY, 0);
}

static size_t spi_hal_transfer(const uint8_t *tx, uint8_t *rx, size_t size) {
    spi_slave_transaction_t t = {};
    t.length    = size * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    // post_setup_cb raise DATA_READY เมื่อ HW พร้อม, transmit รอ master clock
    esp_err_t ret = spi_slave_transmit(SPI2_HOST, &t, pdMS_TO_TICKS(1000));
    gpio_set_level((gpio_num_t)PIN_DATA_READY, 0);
    if (ret != ESP_OK) return 0;
    return (t.trans_len + 7) / 8;
}

// ─────────────────────────────────────────────────────────────────────────────

static uint8_t __attribute__((aligned(4))) s_tx_buf[SPI_COMM_BUF_SIZE];
static uint8_t __attribute__((aligned(4))) s_rx_dma[2][SPI_COMM_BUF_SIZE];
static int s_buf_idx = 0;

// ── Slave response state ──────────────────────────────────────────────────────

typedef enum { RESP_NONE, RESP_INFO, RESP_CHUNK } SlaveResp;

static SlaveResp s_next_resp       = RESP_NONE;
static uint16_t  s_next_chunk_idx  = 0;

// ── Print task ────────────────────────────────────────────────────────────────

static QueueHandle_t s_print_q;

struct PrintMsg {
    bool     rx_ok;
    bool     rx_timeout;  // true = spi_slave_transmit returned 0 (master never clocked)
    uint8_t  rx_cmd;
    uint16_t chunk_idx;
};

static void task_spi(void*);
static void task_print(void*);

// ── App init ──────────────────────────────────────────────────────────────────

void app_spi_init() {
    Serial.begin(115200);
    spi_hal_init();
    gpio_hal_init();
    spi_comm_init();
    s_print_q = xQueueCreate(4, sizeof(PrintMsg));
    xTaskCreatePinnedToCore(task_spi,   "spi",   4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(task_print, "print", 4096, nullptr, 1, nullptr, 1);
}

// ── Main tick ─────────────────────────────────────────────────────────────────

void app_tick() {
    // ── Build TX from previous RX decision ───────────────────────────────
    memset(s_tx_buf, 0, SPI_COMM_BUF_SIZE);

    if (s_next_resp == RESP_INFO) {
        SpiCamInfo info;
        info.width       = DUMMY_IMG_WIDTH;
        info.height      = DUMMY_IMG_HEIGHT;
        info.chunk_count = DUMMY_CHUNK_COUNT;
        info.chunk_size  = DUMMY_CHUNK_SIZE;
        spi_comm_build_tx(SPI_CAM_CMD_INFO, (const uint8_t*)&info, (uint16_t)sizeof(info));

    } else if (s_next_resp == RESP_CHUNK) {
        uint16_t idx      = s_next_chunk_idx;
        bool     is_last  = (idx == (uint16_t)(DUMMY_CHUNK_COUNT - 1u));
        uint16_t data_len = is_last ? (uint16_t)DUMMY_LAST_CHUNK_SIZE : (uint16_t)DUMMY_CHUNK_SIZE;

        uint8_t payload[sizeof(uint16_t) + DUMMY_CHUNK_SIZE];
        memcpy(payload, &idx, sizeof(uint16_t));
        uint8_t base = (uint8_t)((idx * DUMMY_CHUNK_SIZE) & 0xFFu);
        for (uint16_t i = 0; i < data_len; i++) {
            payload[sizeof(uint16_t) + i] = (uint8_t)(base + i);
        }
        spi_comm_build_tx(SPI_CAM_CMD_CHUNK, payload, (uint16_t)(sizeof(uint16_t) + data_len));

    } else {
        spi_comm_build_tx(SPI_CAM_CMD_NONE, nullptr, 0);
    }

    spi_comm_drain_tx(s_tx_buf, SPI_COMM_BUF_SIZE);

    // ── HAL: SPI transfer (DATA_READY managed inside spi_hal_transfer) ───
    size_t received = spi_hal_transfer(s_tx_buf, s_rx_dma[s_buf_idx], SPI_COMM_BUF_SIZE);

    // ── Parse RX → decide next TX ────────────────────────────────────────
    PrintMsg msg = {};
    msg.rx_timeout = (received == 0);

    if (received == 0) {
        spi_comm_flush_rx();  // discard stale bytes so next good tick parses cleanly
    } else {
        spi_comm_push_rx(s_rx_dma[s_buf_idx], (uint16_t)received);
    }
    UDPPacket *pkt = spi_comm_parse_rx();

    msg.rx_ok = (pkt != nullptr);

    if (pkt) {
        msg.rx_cmd = pkt->header->cmd;

        if (pkt->header->cmd == (uint8_t)SPI_CAM_CMD_INFO) {
            s_next_resp = RESP_INFO;

        } else if (pkt->header->cmd == (uint8_t)SPI_CAM_CMD_CHUNK) {
            if (pkt->payload && pkt->header->payload_size >= (uint16_t)sizeof(uint16_t)) {
                memcpy(&s_next_chunk_idx, pkt->payload, sizeof(uint16_t));
                if (s_next_chunk_idx < (uint16_t)DUMMY_CHUNK_COUNT) {
                    s_next_resp   = RESP_CHUNK;
                    msg.chunk_idx = s_next_chunk_idx;
                } else {
                    s_next_resp = RESP_NONE;
                }
            } else {
                s_next_resp = RESP_NONE;
            }
        } else {
            s_next_resp = RESP_NONE;
        }

        FreeUDPPacket(pkt);
    }
    /* On bad parse: keep s_next_resp unchanged.
     * Transient CRC errors should not disrupt the current protocol state —
     * master will retry the same command, and slave will reply correctly.
     * RESP resets to RESP_NONE only when a valid CMD_NONE / unknown cmd is received. */

    xQueueSend(s_print_q, &msg, 0);
    s_buf_idx ^= 1;
}

// ── Tasks ─────────────────────────────────────────────────────────────────────

static void task_spi(void*) {
    while (true) app_tick();
}

static void task_print(void*) {
    PrintMsg msg;
    while (true) {
        if (xQueueReceive(s_print_q, &msg, portMAX_DELAY)) {
            if (!msg.rx_ok) {
                if (msg.rx_timeout)
                    printf("[SPI] transfer timeout (master did not clock)\n");
                else
                    printf("[SPI] bad packet (CRC/signature fail)\n");
            } else if (msg.rx_cmd == (uint8_t)SPI_CAM_CMD_INFO) {
                printf("[SPI] INFO req → sending SpiCamInfo (%ux%u chunks=%u size=%u)\n",
                       DUMMY_IMG_WIDTH, DUMMY_IMG_HEIGHT, DUMMY_CHUNK_COUNT, DUMMY_CHUNK_SIZE);
            } else if (msg.rx_cmd == (uint8_t)SPI_CAM_CMD_CHUNK) {
                bool is_last = (msg.chunk_idx == (uint16_t)(DUMMY_CHUNK_COUNT - 1u));
                printf("[SPI] CHUNK req[%u] → sending %u bytes%s\n",
                       msg.chunk_idx,
                       is_last ? DUMMY_LAST_CHUNK_SIZE : DUMMY_CHUNK_SIZE,
                       is_last ? " (partial)" : "");
            } else {
                printf("[SPI] unknown cmd=0x%02X → CMD_NONE\n", msg.rx_cmd);
            }
        }
    }
}
