#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../warp_rb/ring_buffer.h"
#include "app_create_pack.h"   /* for SPI_FRAME_SIZE */

extern RingBuffer spi_tx_rb;

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate spi_tx_rb (3-frame capacity). Call once at startup. */
bool spi_rb_init(void);

/* Push one serialized frame (SPI_FRAME_SIZE bytes) atomically. */
bool spi_rb_push(const uint8_t* frame);

/* Pull one frame into out_buf. Returns false if no full frame available. */
bool spi_rb_pull(uint8_t* out_buf);

/* True when at least one complete frame is ready to pull. */
bool spi_rb_has_frame(void);

#ifdef __cplusplus
}
#endif
