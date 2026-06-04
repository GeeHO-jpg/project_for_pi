#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void   spi_hal_init(void);
size_t spi_hal_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

#ifdef __cplusplus
}
#endif
