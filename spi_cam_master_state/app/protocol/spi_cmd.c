#include "spi_cmd.h"
#include <string.h>

uint16_t spi_cmd_pack_info_req(uint8_t *payload)
{
    (void)payload;
    return 0;   /* INFO request carries no payload */
}

uint16_t spi_cmd_pack_chunk_req(uint8_t *payload, uint16_t chunk_index)
{
    SpiChunkReq req;
    req.chunk_index = chunk_index;
    memcpy(payload, &req, sizeof(req));
    return (uint16_t)sizeof(req);
}

bool spi_cmd_unpack_info_resp(const uint8_t *payload, uint16_t len, SpiCamInfo *out)
{
    if (!payload || !out || len < (uint16_t)sizeof(SpiCamInfo))
        return false;
    memcpy(out, payload, sizeof(SpiCamInfo));
    return true;
}

bool spi_cmd_unpack_chunk_resp(const uint8_t *payload, uint16_t len, SpiChunkResp *out)
{
    if (!payload || !out || len < (uint16_t)sizeof(uint16_t))
        return false;

    uint16_t data_len = len - (uint16_t)sizeof(uint16_t);
    if (data_len > SPI_CHUNK_DATA_MAX)
        return false;

    memcpy(&out->chunk_index, payload, sizeof(uint16_t));
    memcpy(out->data, payload + sizeof(uint16_t), data_len);
    return true;
}
