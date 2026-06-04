/*
 * ring_buffer.c
 *
 *  Updated for safer ISR + task usage
 */

#include "ring_buffer.h"
#include "cmsis_gcc.h"

#include <stdlib.h>
#include <string.h>

static uint32_t rb_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void rb_exit_critical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

static uint16_t next_index(RingBuffer* rb, uint16_t index)
{
    return (uint16_t)((index + 1U) % rb->size);
}

int rb_init(RingBuffer* rb, uint16_t size)
{
    if (rb == NULL || size < 2U) {
        return -1;
    }

    rb->buffer = (uint8_t*)malloc(size);
    if (rb->buffer == NULL) {
        return -1;
    }

    rb->size = size;

    uint32_t primask = rb_enter_critical();
    rb->head = 0U;
    rb->tail = 0U;
    rb_exit_critical(primask);

    return 0;
}

void rb_free(RingBuffer* rb)
{
    if (rb == NULL) {
        return;
    }

    if (rb->buffer != NULL) {
        free(rb->buffer);
        rb->buffer = NULL;
    }

    uint32_t primask = rb_enter_critical();
    rb->size = 0U;
    rb->head = 0U;
    rb->tail = 0U;
    rb_exit_critical(primask);
}

uint16_t rb_used(RingBuffer* rb)
{
    uint16_t head;
    uint16_t tail;
    uint16_t used;

    if (rb == NULL || rb->buffer == NULL || rb->size < 2U) {
        return 0U;
    }

    uint32_t primask = rb_enter_critical();
    head = rb->head;
    tail = rb->tail;
    rb_exit_critical(primask);

    if (head >= tail) {
        used = (uint16_t)(head - tail);
    } else {
        used = (uint16_t)(rb->size - tail + head);
    }

    return used;
}

uint16_t rb_available(RingBuffer* rb)
{
    if (rb == NULL || rb->buffer == NULL || rb->size < 2U) {
        return 0U;
    }

    return (uint16_t)(rb->size - rb_used(rb) - 1U);
}

bool rb_put_byte(RingBuffer* rb, uint8_t data)
{
    uint16_t next;

    if (rb == NULL || rb->buffer == NULL || rb->size < 2U) {
        return false;
    }

    uint32_t primask = rb_enter_critical();

    next = next_index(rb, rb->head);
    if (next == rb->tail) {
        rb_exit_critical(primask);
        return false;
    }

    rb->buffer[rb->head] = data;
    rb->head = next;

    rb_exit_critical(primask);
    return true;
}

uint16_t rb_put(RingBuffer* rb, const uint8_t* data, uint16_t len)
{
    uint16_t written = 0U;

    if (rb == NULL || rb->buffer == NULL || data == NULL || len == 0U) {
        return 0U;
    }

    for (uint16_t i = 0U; i < len; i++) {
        if (!rb_put_byte(rb, data[i])) {
            break;
        }

        written++;
    }

    return written;
}

bool rb_put_exact(RingBuffer* rb, const uint8_t* data, uint16_t len)
{
    if (rb == NULL || rb->buffer == NULL || data == NULL || len == 0U) {
        return false;
    }

    uint32_t primask = rb_enter_critical();

    uint16_t available;
    if (rb->head >= rb->tail) {
        available = (uint16_t)(rb->size - (rb->head - rb->tail) - 1U);
    } else {
        available = (uint16_t)(rb->tail - rb->head - 1U);
    }

    if (available < len) {
        rb_exit_critical(primask);
        return false;
    }

    for (uint16_t i = 0U; i < len; ++i) {
        rb->buffer[rb->head] = data[i];
        rb->head = next_index(rb, rb->head);
    }

    rb_exit_critical(primask);
    return true;
}

uint16_t rb_get(RingBuffer* rb, uint8_t* data, uint16_t len)
{
    uint16_t count = 0U;

    if (rb == NULL || rb->buffer == NULL || data == NULL || len == 0U) {
        return 0U;
    }

    uint32_t primask = rb_enter_critical();

    while ((rb->tail != rb->head) && (count < len)) {
        data[count] = rb->buffer[rb->tail];
        rb->tail = next_index(rb, rb->tail);
        count++;
    }

    rb_exit_critical(primask);
    return count;
}

uint16_t rb_get_count(RingBuffer* rb)
{
    return rb_used(rb);
}

void rb_flush(RingBuffer* rb)
{
    if (rb == NULL || rb->buffer == NULL) {
        return;
    }

    uint32_t primask = rb_enter_critical();
    rb->head = 0U;
    rb->tail = 0U;
    rb_exit_critical(primask);
}

uint8_t rb_peek_at(const RingBuffer *rb, uint16_t offset, uint8_t *out)
{
    uint16_t count;
    uint16_t index;

    if ((rb == NULL) || (out == NULL))
    {
        return 0U;
    }

    count = rb_get_count((RingBuffer *)rb);
    if (offset >= count)
    {
        return 0U;
    }

    index = (uint16_t)((rb->tail + offset) % rb->size);
    *out = rb->buffer[index];
    return 1U;
}
