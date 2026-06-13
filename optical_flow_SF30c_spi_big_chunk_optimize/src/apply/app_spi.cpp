#include "app_spi.h"
#include "spi_comm.h"
#include "protocal/Packet_RCSA/Commands.h"
#include "../config/param_conf.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <cstring>
#include <cstdio>

#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "chunk_manage.h"

#define CHUNK_SIZE 2
#define FRAME_SIZE (9 + CHUNK_SIZE + 4)

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
    // queue_size=2: เผื่อ transaction ใหม่ให้คิวได้แม้ transaction ก่อนหน้ายังรอ master clock อยู่
    // (queue_size=1 เคยทำให้ spi_slave_transmit() ค้างถาวรถ้า master จังหวะไม่ตรงพอดี ต้องรีเซ็ตบอร์ด)
    slv_cfg.queue_size   = 2;
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



// ─────────────────────────────────────────────────────────────────────────────

void gpio_hal_set_ready(bool ready) {
    gpio_set_level((gpio_num_t)PIN_DATA_READY, ready ? 1 : 0);
}

size_t spi_hal_transfer(const uint8_t *tx, uint8_t *rx, size_t size) {
    spi_slave_transaction_t t = {};
    t.length    = size * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t ret = spi_slave_transmit(SPI2_HOST, &t, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return 0;
    return (t.trans_len + 7) / 8;
}

void app_spi_init() {
    // Serial.begin(115200);
    spi_hal_init();
    gpio_hal_init();
    spi_comm_init();
}

