#include "ring_buffer.h"

static uint16_t rb_next(const RingBuffer *rb, uint16_t i) {
    return (uint16_t)((i + 1u) % rb->size);
}

int rb_init(RingBuffer *rb, uint16_t size) {
    if (!rb || size < 2) return -1;
    rb->buf = (uint8_t*)malloc(size);
    if (!rb->buf) return -1;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    return 0;
}

void rb_free(RingBuffer *rb) {
    if (!rb) return;
    free(rb->buf);
    rb->buf  = NULL;
    rb->size = rb->head = rb->tail = 0;
}

void rb_flush(RingBuffer *rb) {
    if (rb) rb->head = rb->tail = 0;
}

uint16_t rb_used(const RingBuffer *rb) {
    if (!rb || !rb->buf) return 0;
    if (rb->head >= rb->tail) return rb->head - rb->tail;
    return (uint16_t)(rb->size - rb->tail + rb->head);
}

uint16_t rb_available(const RingBuffer *rb) {
    if (!rb || !rb->buf) return 0;
    return (uint16_t)(rb->size - rb_used(rb) - 1u);
}

bool rb_put_byte(RingBuffer *rb, uint8_t b) {
    if (!rb || !rb->buf) return false;
    uint16_t next = rb_next(rb, rb->head);
    if (next == rb->tail) return false;
    rb->buf[rb->head] = b;
    rb->head = next;
    return true;
}

uint16_t rb_put(RingBuffer *rb, const uint8_t *data, uint16_t len) {
    uint16_t n = 0;
    for (; n < len; n++) {
        if (!rb_put_byte(rb, data[n])) break;
    }
    return n;
}

bool rb_get_byte(RingBuffer *rb, uint8_t *b) {
    if (!rb || !rb->buf || rb->head == rb->tail) return false;
    *b = rb->buf[rb->tail];
    rb->tail = rb_next(rb, rb->tail);
    return true;
}

uint16_t rb_get(RingBuffer *rb, uint8_t *data, uint16_t len) {
    uint16_t n = 0;
    for (; n < len; n++) {
        if (!rb_get_byte(rb, &data[n])) break;
    }
    return n;
}
