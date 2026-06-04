/*
 * CircularBuffer.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_CIRCULARBUFFER_H_
#define INC_PACKET_HEADER_CIRCULARBUFFER_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint8_t* array;
    size_t size;
    size_t head_index;
} CircularBuffer;

CircularBuffer* CreateCircularBuffer(size_t size);
void FreeCircularBuffer(CircularBuffer* buffer);

bool ResetCircularBuffer(CircularBuffer* buffer);
bool IsOperableCircularBuffer(CircularBuffer* buffer);
bool AppendCircularBuffer(CircularBuffer* buffer, uint8_t in_byte);

uint8_t PeekCircularBufferHeadByte(CircularBuffer* buffer);
uint8_t* ReadCircularBuffer(CircularBuffer* buffer);


#endif /* INC_PACKET_HEADER_CIRCULARBUFFER_H_ */
