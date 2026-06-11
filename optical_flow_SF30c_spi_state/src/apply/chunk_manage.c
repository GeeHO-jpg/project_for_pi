#include "chunk_manage.h"
#include <stdint.h>
#include <string.h>


static uint16_t last_index_payload = 0;

uint16_t total_chunks(uint16_t payload_size, uint16_t chunk_size)
{
    if (chunk_size == 0) return 0;
    return (uint16_t)((payload_size + chunk_size - 1) / chunk_size);
}

uint16_t select_chunk(uint8_t* chunk_buf, const uint8_t *payload, uint16_t payload_size, uint16_t chunk_size)
{
    uint16_t remaining = payload_size - last_index_payload;
    uint16_t n = (remaining < chunk_size) ? remaining : chunk_size;

    memcpy(chunk_buf, &payload[last_index_payload], n);
    last_index_payload += n;

    if (last_index_payload >= payload_size) {
        last_index_payload = 0;
    }

    return n;
}

uint16_t get_chunk(uint8_t* chunk_buf, const uint8_t *payload, uint16_t payload_size, uint16_t chunk_size, uint16_t chunk_index)
{
    uint16_t offset = (uint16_t)(chunk_index * chunk_size);
    if (offset >= payload_size) return 0;

    uint16_t remaining = payload_size - offset;
    uint16_t n = (remaining < chunk_size) ? remaining : chunk_size;

    memcpy(chunk_buf, &payload[offset], n);
    return n;
}