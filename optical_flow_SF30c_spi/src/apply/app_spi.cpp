#include "app_spi.h"
#include "spi_comm.h"
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

static void spi_hal_init() {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num   = PIN_MOSI;
    bus_cfg.miso_io_num   = PIN_MISO;
    bus_cfg.sclk_io_num   = PIN_SCK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;

    spi_slave_interface_config_t slv_cfg = {};
    slv_cfg.spics_io_num = PIN_CS;
    slv_cfg.flags        = 0;
    slv_cfg.queue_size   = 1;
    slv_cfg.mode         = 0;

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

static void gpio_hal_set_ready(bool ready) {
    gpio_set_level((gpio_num_t)PIN_DATA_READY, ready ? 1 : 0);
}

static size_t spi_hal_transfer(const uint8_t *tx, uint8_t *rx, size_t size) {
    spi_slave_transaction_t t = {};
    t.length    = size * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t ret = spi_slave_transmit(SPI2_HOST, &t, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return 0;
    return (t.trans_len + 7) / 8;
}

// ─────────────────────────────────────────────────────────────────────────────

static uint8_t __attribute__((aligned(4))) s_tx_buf[SPI_COMM_BUF_SIZE];
static uint8_t __attribute__((aligned(4))) s_rx_dma[2][SPI_COMM_BUF_SIZE];
static int s_buf_idx = 0;

static QueueHandle_t s_print_q;

struct PrintMsg {
    bool     ok;
    uint16_t id;
    uint8_t  cmd;
};

static const uint8_t k_payload[SPI_COMM_PAYLOAD_SIZE] = {
    0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
    0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
    0xEF, 0xEE, 0xED
};

static void task_spi(void*);
static void task_print(void*);

void app_spi_init() {
    Serial.begin(115200);
    spi_hal_init();
    gpio_hal_init();
    spi_comm_init();
    s_print_q = xQueueCreate(4, sizeof(PrintMsg));
    xTaskCreatePinnedToCore(task_spi,   "spi",   4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(task_print, "print", 4096, nullptr, 1, nullptr, 1);
}

void app_tick() {
    // ── Driver: build TX packet → TX ring buffer ──────────────────────────
    spi_comm_build_tx(0x01, k_payload, SPI_COMM_PAYLOAD_SIZE);

    // ── Driver: drain TX ring buffer → flat buffer ────────────────────────
    memset(s_tx_buf, 0, SPI_COMM_BUF_SIZE);
    spi_comm_drain_tx(s_tx_buf, SPI_COMM_BUF_SIZE);

    // ── HAL: signal ready → SPI transfer → clear ready ───────────────────
    gpio_hal_set_ready(true);
    size_t received = spi_hal_transfer(s_tx_buf, s_rx_dma[s_buf_idx], SPI_COMM_BUF_SIZE);
    gpio_hal_set_ready(false);

    // ── Driver: push RX bytes → RX ring buffer → parse ────────────────────
    spi_comm_push_rx(s_rx_dma[s_buf_idx], (uint16_t)received);
    UDPPacket* pkt = spi_comm_parse_rx();

    // ── Notify print task ─────────────────────────────────────────────────
    PrintMsg msg = { pkt != nullptr, 0, 0 };
    if (pkt) {
        msg.id  = pkt->header->id;
        msg.cmd = pkt->header->cmd;
        FreeUDPPacket(pkt);
    }
    xQueueSend(s_print_q, &msg, 0);

    s_buf_idx ^= 1;
}

static void task_spi(void*) {
    while (true) app_tick();
}

static void task_print(void*) {
    PrintMsg msg;
    while (true) {
        if (xQueueReceive(s_print_q, &msg, portMAX_DELAY)) {
            if (msg.ok)
                printf("[RX] OK  id=%u cmd=0x%02X\n", msg.id, msg.cmd);
            else
                printf("[RX] FAIL (bad signature or CRC)\n");
        }
    }
}
