/*
 * CircularBuffer.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#include "CircularBuffer.h"

// Function to create and initialize a CircularBuffer
// Parameters: size - the maximum size of the buffer
// Returns: A pointer to the created CircularBuffer, or NULL on failure
CircularBuffer* CreateCircularBuffer(size_t size)
{
    CircularBuffer* buffer = (CircularBuffer*)malloc(sizeof(CircularBuffer));
    if (buffer == NULL)  // Check if memory allocation for the buffer struct failed
        return NULL;
    
    buffer->size = size;
    buffer->array = (uint8_t*)malloc(size * sizeof(uint8_t));
    if (buffer->array == NULL)  // Check if memory allocation for the array failed
    {
        free(buffer);  // Free the previously allocated buffer struct memory
        return NULL;
    }
 


    ResetCircularBuffer(buffer);  // Initialize buffer indices and clear the buffer
    return buffer;
}

// Function to deallocate the CircularBuffer and its internal array
// Parameters: buffer - a pointer to the CircularBuffer to deallocate
void FreeCircularBuffer(CircularBuffer* buffer)
{
    if (buffer != NULL) 
    {
        if (buffer->array != NULL) 
        {
            free(buffer->array);  // Free the internal array
            buffer->array = NULL;  // Set the pointer to NULL to avoid dangling references
        }
        
        free(buffer);  // Free the CircularBuffer structure itself
    }
}

// Function to reset the buffer to its initial state
// Parameters: buffer - a pointer to the CircularBuffer to reset
// Returns: true if the buffer was successfully reset, false if the buffer is not operable
bool ResetCircularBuffer(CircularBuffer* buffer)
{
    if (!IsOperableCircularBuffer(buffer))  // Check if the buffer is valid and operable
        return false;
    
    buffer->head_index = 0;  // Start with head at index 0
    buffer->array[buffer->head_index] = 0;  // Initialize the first element to 0 (optional, can be omitted)
    
    return true;  // Buffer successfully reset
}

// Function to check if the buffer is operable
// Parameters: buffer - a pointer to the CircularBuffer to check
// Returns: true if the buffer is valid and can be operated on, false otherwise
bool IsOperableCircularBuffer(CircularBuffer* buffer)
{
    // The buffer is operable if it's not NULL, has a size greater than 0, and its array is allocated
    return (buffer != NULL && buffer->size > 0 && buffer->array != NULL);
}

// Function to add a byte to the buffer (with automatic dequeue of the oldest data if full)
// Parameters: buffer - a pointer to the CircularBuffer
//             in_byte - the byte to be added to the buffer
// Returns: true if the byte was successfully added, false if the buffer is not operable
bool AppendCircularBuffer(CircularBuffer* buffer, uint8_t in_byte)
{
    if (!IsOperableCircularBuffer(buffer))  // Check if the buffer is valid and operable
        return false;
    
    // Move the head to the next position in the circular buffer (overwrites the oldest data if full)
    buffer->array[buffer->head_index] = in_byte;  // Insert the new byte
    buffer->head_index = (buffer->head_index + 1) % buffer->size;

    return true;  // Byte successfully added to the buffer
}

// Function to peek at the first (oldest) byte in the CircularBuffer without modifying it
// Parameters: buffer - a pointer to the CircularBuffer to peek from
// Returns: The first byte (oldest byte) in the CircularBuffer, or 0 if the buffer is not operable
uint8_t PeekCircularBufferHeadByte(CircularBuffer* buffer)
{
    if (!IsOperableCircularBuffer(buffer))  // Check if the buffer is valid and operable
        return 0;
    
    return buffer->array[buffer->head_index];
}

// Function to read the entire buffer as an array
// Parameters: buffer - a pointer to the CircularBuffer to read from
// Returns: A dynamically allocated array containing the buffer's data, or NULL if the buffer is not operable
// Note: The caller is responsible for freeing the returned array
uint8_t* ReadCircularBuffer(CircularBuffer* buffer)
{
    if (!IsOperableCircularBuffer(buffer))  // Check if the buffer is valid and operable
        return NULL;

    // Allocate new memory for the array to hold the buffer's contents
    uint8_t* array = (uint8_t*)malloc(buffer->size * sizeof(uint8_t));
    if (array == NULL)
        return NULL;  // Memory allocation failed

    size_t i = buffer->head_index;  // Start reading from the current head index
    for (size_t j = 0; j < buffer->size; j++)
    {
        array[j] = buffer->array[i];  // Copy data from the circular buffer to the new array
        i = (i + 1) % buffer->size;   // Move to the next element, wrapping around if necessary
    }

    return array;  // Return the array containing the buffer's data
}
