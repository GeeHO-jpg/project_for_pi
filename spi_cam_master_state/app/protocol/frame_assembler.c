#include "frame_assembler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint8_t    *s_buf         = NULL;
static uint32_t    s_buf_size    = 0;
static uint16_t    s_chunk_count = 0;
static uint16_t    s_chunk_size  = 0;
static uint16_t    s_received    = 0;
static frame_cb_t  s_cb          = NULL;

void frame_asm_init(uint16_t width, uint16_t height,
                    uint16_t chunk_count, uint16_t chunk_size,
                    frame_cb_t cb)
{
    frame_asm_reset();

    s_buf_size    = (uint32_t)width * height;
    s_chunk_count = chunk_count;
    s_chunk_size  = chunk_size;
    s_received    = 0;
    s_cb          = cb;

    s_buf = (uint8_t *)malloc(s_buf_size);
    if (!s_buf) {
        fprintf(stderr, "[frame_asm] malloc failed (%u bytes)\n", (unsigned)s_buf_size);
        s_buf_size = 0;
    }
}

bool frame_asm_add_chunk(uint16_t chunk_index, const uint8_t *data, uint16_t data_len)
{
    if (!s_buf || !data || chunk_index >= s_chunk_count)
        return false;

    uint32_t offset   = (uint32_t)chunk_index * s_chunk_size;
    uint32_t copy_len = data_len;
    if (offset + copy_len > s_buf_size)
        copy_len = s_buf_size - offset;

    memcpy(s_buf + offset, data, copy_len);
    s_received++;

    if (s_received >= s_chunk_count) {
        if (s_cb) s_cb(s_buf, s_buf_size);
        return true;
    }
    return false;
}

void frame_asm_reset(void)
{
    free(s_buf);
    s_buf         = NULL;
    s_buf_size    = 0;
    s_chunk_count = 0;
    s_chunk_size  = 0;
    s_received    = 0;
    s_cb          = NULL;
}
