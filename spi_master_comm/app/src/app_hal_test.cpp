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
    while (true) {
        uint8_t tx = 0x3A;
        uint8_t rx = g_spi->transferByte(tx);
        printf("[SPI] TX: 0x%02X  RX: 0x%02X\n", tx, rx);
        usleep(1000); // 1ms
    }
}
