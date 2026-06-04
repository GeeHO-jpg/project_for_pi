#include <Arduino.h>
#include <ESP32SPISlave.h>
#include "config.hpp"

static ESP32SPISlave slave;

static constexpr size_t BUF_SIZE = 32;
static uint8_t __attribute__((aligned(4))) tx_buf[BUF_SIZE];
static uint8_t __attribute__((aligned(4))) rx_buf[BUF_SIZE];

static void task_spi(void* arg);

void setup() {
    Serial.begin(115200);

    // begin(bus, sck, miso, mosi, ss)
    slave.setDataMode(SPI_MODE0);
    slave.begin(FSPI, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    for (size_t i = 0; i < BUF_SIZE; i++) tx_buf[i] = (0xFF - i) & 0xFF;
    memset(rx_buf, 0, BUF_SIZE);

    xTaskCreatePinnedToCore(task_spi, "spi", 4096, nullptr, 2, nullptr, 0);
}

void loop() {}

// v0.4.x API: transfer(tx, rx, size) — blocking, size ต้องหาร 4 ลงตัว
static void task_spi(void* arg) {
    while (true) {
        size_t received = slave.transfer(tx_buf, rx_buf, BUF_SIZE);

        printf("[SPI] transfer done, received=%u\n", received);
        for (size_t i = 0; i < received; i++) printf(" %02X", rx_buf[i]);
        printf("\n");
    }
}
