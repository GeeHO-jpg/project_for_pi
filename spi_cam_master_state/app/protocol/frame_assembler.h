#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Called once a complete frame is assembled.
   buf is valid only until frame_asm_reset() is called — copy if you need to keep it. */
typedef void (*frame_cb_t)(const uint8_t *buf, uint32_t size);

/* Allocate frame buffer and set assembly parameters.
   Must be called before frame_asm_add_chunk. */
void frame_asm_init(uint16_t width, uint16_t height,
                    uint16_t chunk_count, uint16_t chunk_size,
                    frame_cb_t cb);

/* Write one chunk into the frame buffer.
   Returns true when all chunks have been received (frame complete). */
bool frame_asm_add_chunk(uint16_t chunk_index, const uint8_t *data, uint16_t data_len);

/* Free the frame buffer and reset all state. */
void frame_asm_reset(void);

#ifdef __cplusplus
}
#endif
