#include "spi_hal.h"
#include <ESP32SPISlave.h>
#include "config.hpp"

static ESP32SPISlave s_slave;

void spi_hal_init(void) {
    s_slave.setDataMode(SPI_MODE0);
    s_slave.begin(FSPI, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
}

size_t spi_hal_transfer(const uint8_t *tx, uint8_t *rx, size_t len) {
    return s_slave.transfer(const_cast<uint8_t*>(tx), rx, len);
}
