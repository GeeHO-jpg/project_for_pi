/*
 * CircularBuffer.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#include "CircularBuffer.h"

CircularBuffer* CreateCircularBuffer(size_t size)
{
    CircularBuffer* buffer = (CircularBuffer*)malloc(sizeof(CircularBuffer));
    if (buffer == NULL)
        return NULL;

    buffer->size = size;
    buffer->array = (uint8_t*)malloc(size * sizeof(uint8_t));
    if (buffer->array == NULL)
    {
        free(buffer);
        return NULL;
    }

    ResetCircularBuffer(buffer);
    return buffer;
}

void FreeCircularBuffer(CircularBuffer* buffer)
{
    if (buffer != NULL)
    {
        if (buffer->array != NULL)
        {
            free(buffer->array);
            buffer->array = NULL;
        }
        free(buffer);
    }
}

bool ResetCircularBuffer(CircularBuffer* buffer)
{
    if (!IsOperableCircularBuffer(buffer))
        return false;

    buffer->head_index = 0;
    buffer->array[buffer->head_index] = 0;

    return true;
}

bool IsOperableCircularBuffer(CircularBuffer* buffer)
{
    return (buffer != NULL && buffer->size > 0 && buffer->array != NULL);
}

bool AppendCircularBuffer(CircularBuffer* buffer, uint8_t in_byte)
{
    if (!IsOperableCircularBuffer(buffer))
        return false;

    buffer->array[buffer->head_index] = in_byte;
    buffer->head_index = (buffer->head_index + 1) % buffer->size;

    return true;
}

uint8_t PeekCircularBufferHeadByte(CircularBuffer* buffer)
{
    if (!IsOperableCircularBuffer(buffer))
        return 0;

    return buffer->array[buffer->head_index];
}

uint8_t* ReadCircularBuffer(CircularBuffer* buffer)
{
    if (!IsOperableCircularBuffer(buffer))
        return NULL;

    uint8_t* array = (uint8_t*)malloc(buffer->size * sizeof(uint8_t));
    if (array == NULL)
        return NULL;

    size_t i = buffer->head_index;
    for (size_t j = 0; j < buffer->size; j++)
    {
        array[j] = buffer->array[i];
        i = (i + 1) % buffer->size;
    }

    return array;
}
