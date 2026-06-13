#pragma once
#include <stdint.h>
#include <stddef.h>

void app_spi_init();
size_t spi_hal_transfer(const uint8_t *tx, uint8_t *rx, size_t size);
void gpio_hal_set_ready(bool ready);