#include "app_hal_test.h"
#include "hal/include/spi_bus.hpp"

#include <cstdio>
#include <unistd.h>

static hal::SPIBus* g_spi = nullptr;

void test_hal_spi_init(void)
{
    g_spi = new hal::SPIBus("/dev/spidev0.0", 1000000);
}

void test_hal_spi_run(void)
{
    uint8_t tx[32] = {};
    uint8_t rx[32] = {};

    for (size_t i = 0; i < sizeof(tx); i++) tx[i] = i;

    while (true) {
        g_spi->transfer(tx, rx, sizeof(tx));

        printf("[SPI] TX:");
        for (size_t i = 0; i < sizeof(tx); i++) printf(" %02X", tx[i]);
        printf("\n[SPI] RX:");
        for (size_t i = 0; i < sizeof(rx); i++) printf(" %02X", rx[i]);
        printf("\n\n");

        usleep(1000); // 1ms
    }
}
