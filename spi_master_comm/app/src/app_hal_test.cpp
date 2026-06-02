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
        g_spi->transferByte(0xAA);
        usleep(1000); // 1ms
    }
}
