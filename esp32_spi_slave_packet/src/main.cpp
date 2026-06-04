#include <Arduino.h>
#include <ESP32SPISlave.h>
#include <string.h>
#include "config.hpp"
#include "Packet_RCSA/common_defs.h"
#include "apply/spi_image.h"
#include "apply/app_spi_rb.h"

static ESP32SPISlave slave;

static uint8_t __attribute__((aligned(4))) tx_buf[SPI_FRAME_SIZE];
static uint8_t __attribute__((aligned(4))) rx_buf[SPI_FRAME_SIZE];

static void task_producer(void* arg);
static void task_spi(void* arg);

void setup()
{
    Serial.begin(115200);
    printf("[SETUP] start\n");

    printf("[SETUP] spi_image_init...\n");
    spi_image_init();
    printf("[SETUP] spi_image_init done\n");

    slave.setDataMode(SPI_MODE0);
    printf("[SETUP] slave.begin...\n");
    slave.begin(FSPI, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    printf("[SETUP] slave.begin done\n");

    xTaskCreatePinnedToCore(task_producer, "producer", 4096*4, nullptr, 1, nullptr, 1);
    printf("[SETUP] producer created\n");
    xTaskCreatePinnedToCore(task_spi,      "spi",      4096*4, nullptr, 2, nullptr, 0);
    printf("[SETUP] spi task created\n");
}

void loop() {}

/* ─── Producer: create packets → push into ring buffer ──────────────────── */

static void task_producer(void* arg)
{
    while (!spi_image_push_info()) vTaskDelay(pdMS_TO_TICKS(1));
    printf("[PROD] image info queued\n");

    for (uint16_t chunk = 0; chunk < IMAGE_NUM_CHUNKS; chunk++) {
        while (!spi_image_push_chunk(chunk)) vTaskDelay(pdMS_TO_TICKS(1));
        printf("[PROD] chunk %3u queued\n", chunk);
    }

    printf("[PROD] done\n");
    vTaskDelete(nullptr);
}

/* ─── SPI Consumer: pull from ring buffer → send via SPI ────────────────── */

static void task_spi(void* arg)
{
    while (true) {
        if (!spi_rb_pull(tx_buf)) 
        {
            memset(tx_buf, 0, SPI_FRAME_SIZE);
        }

        size_t received = slave.transfer(tx_buf, rx_buf, SPI_FRAME_SIZE);
        printf("[SPI] transfer done, received=%u, cmd=0x%02X\n",
               received, tx_buf[6]);
    }
}
