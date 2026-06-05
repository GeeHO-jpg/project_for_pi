#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SPI camera protocol command IDs — must match slave's SpiCamCmd enum
 * (optical_flow_SF30c_spi_state/src/apply/spi_cam_cmd.h) */
typedef enum {
    SPI_CMD_NONE      = 0,
    SPI_CMD_CAM_INFO  = 1,
    SPI_CMD_CAM_CHUNK = 2,
} SpiCmd;

/* ── Payload structs (packed, no padding) ─────────────────────────────────── */

/* INFO response: slave → master */
#pragma pack(push, 1)
typedef struct {
    uint16_t width;        /* image width  in pixels */
    uint16_t height;       /* image height in pixels */
    uint16_t chunk_count;  /* total number of chunks */
    uint16_t chunk_size;   /* image bytes per chunk (excluding chunk_index field) */
} SpiCamInfo;

/* CHUNK request: master → slave, carried in TX payload */
typedef struct {
    uint16_t chunk_index;
} SpiChunkReq;

/* CHUNK response: slave → master, carried in RX payload */
#define SPI_CHUNK_DATA_MAX  ((uint16_t)(SPI_COMM_PAYLOAD_SIZE - sizeof(uint16_t)))  /* 17 */

typedef struct {
    uint16_t chunk_index;
    uint8_t  data[SPI_CHUNK_DATA_MAX];
} SpiChunkResp;
#pragma pack(pop)

/* ── Pack helpers: write into payload buf, return byte count written ─────── */
uint16_t spi_cmd_pack_info_req (uint8_t *payload);
uint16_t spi_cmd_pack_chunk_req(uint8_t *payload, uint16_t chunk_index);

/* ── Unpack helpers: parse from UDPPacket payload bytes ─────────────────── */
bool spi_cmd_unpack_info_resp (const uint8_t *payload, uint16_t len, SpiCamInfo   *out);
bool spi_cmd_unpack_chunk_resp(const uint8_t *payload, uint16_t len, SpiChunkResp *out);

#ifdef __cplusplus
}
#endif
