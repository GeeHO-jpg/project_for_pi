#pragma once

#include <stdint.h>
#include "spi_cam_cmd.h"
#include "spi_comm.h"

// Dummy image parameters
#define DUMMY_IMG_WIDTH       320u
#define DUMMY_IMG_HEIGHT      240u
#define DUMMY_CHUNK_COUNT     5u
#define DUMMY_CHUNK_SIZE      ((uint16_t)(SPI_COMM_PAYLOAD_SIZE - sizeof(uint16_t)))  // 17
#define DUMMY_LAST_CHUNK_SIZE 7u   // ปรับได้ — chunk สุดท้ายจงใจไม่เต็ม

#pragma pack(push, 1)
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t chunk_count;
    uint16_t chunk_size;
} SpiCamInfo;

typedef struct {
    uint16_t chunk_index;
    uint8_t  data[DUMMY_CHUNK_SIZE];
} SpiChunkResp;
#pragma pack(pop)
