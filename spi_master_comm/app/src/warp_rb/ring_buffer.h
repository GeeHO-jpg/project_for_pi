/*
 * ring_buffer.h
 *
 *  Created on: Mar 7, 2026
 *      Author: mind
 */

#ifndef INC_RING_BUFFER_H_
#define INC_RING_BUFFER_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 512

typedef struct
{
    uint8_t *buffer;
    uint16_t size;

    volatile uint16_t head;
    volatile uint16_t tail;

} RingBuffer;


/* init buffer */
int rb_init(RingBuffer *rb, uint16_t size);

/* free buffer */
void rb_free(RingBuffer *rb);

/* bytes currently stored (Same as rb_used) */
uint16_t rb_get_count(RingBuffer *rb);

/* bytes currently stored */
uint16_t rb_used(RingBuffer *rb);

/* free space */
uint16_t rb_available(RingBuffer *rb);

/* Clear all data in buffer */
void rb_flush(RingBuffer *rb);

/* put data (overwrite old data if full) */
uint16_t rb_put(RingBuffer *rb, const uint8_t *data, uint16_t len);

/* put a full block atomically, fail if there is not enough space */
bool rb_put_exact(RingBuffer *rb, const uint8_t *data, uint16_t len);

/* get data */
uint16_t rb_get(RingBuffer *rb, uint8_t *data, uint16_t len);

/* put single byte (good for ISR) */
bool rb_put_byte(RingBuffer *rb, uint8_t data);

/*find peek of packet size*/
uint8_t rb_peek_at(const RingBuffer *rb, uint16_t offset, uint8_t *out);


#ifdef __cplusplus
}
#endif

#endif /* INC_RING_BUFFER_H_ */
