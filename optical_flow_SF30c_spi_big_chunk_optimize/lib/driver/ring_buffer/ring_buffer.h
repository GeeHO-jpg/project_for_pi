#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  *buf;
    uint16_t  size;   // allocated bytes (capacity + 1)
    uint16_t  head;   // write index
    uint16_t  tail;   // read index
} RingBuffer;

// ── lifecycle ──────────────────────────────────────────────────────────────────
int  rb_init(RingBuffer *rb, uint16_t size);
void rb_free(RingBuffer *rb);
void rb_flush(RingBuffer *rb);

// ── status ─────────────────────────────────────────────────────────────────────
uint16_t rb_used(const RingBuffer *rb);
uint16_t rb_available(const RingBuffer *rb);

// ── write ──────────────────────────────────────────────────────────────────────
bool     rb_put_byte(RingBuffer *rb, uint8_t b);
uint16_t rb_put(RingBuffer *rb, const uint8_t *data, uint16_t len);

// ── read ───────────────────────────────────────────────────────────────────────
bool     rb_get_byte(RingBuffer *rb, uint8_t *b);
uint16_t rb_get(RingBuffer *rb, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
