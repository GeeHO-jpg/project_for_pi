#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Init ring buffer. Call once before any push. */
bool spi_image_init(void);

/* Create image-info packet and push into spi_tx_rb. */
bool spi_image_push_info(void);

/* Create chunk packet for chunk_idx and push into spi_tx_rb. */
bool spi_image_push_chunk(uint16_t chunk_idx);

#ifdef __cplusplus
}
#endif
