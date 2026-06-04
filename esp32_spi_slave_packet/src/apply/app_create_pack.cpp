#include "app_create_pack.h"
#include "../Packet_RCSA/common_defs.h"
#include "../Packet_RCSA/UDPPacketHeader.h"
#include "../Packet_RCSA/UDPPacket.h"
#include "../Packet_RCSA/crc_32.h"
#include <string.h>
#include <stdlib.h>

#define DEVICE_ID  0x0001u

/* ── internal: UDPPacket → flat byte frame ───────────────────────────────── */

static bool serialize_packet(UDPPacket* pkt, uint8_t* out_frame)
{
    memset(out_frame, 0, SPI_FRAME_SIZE);

    /* header */
    uint8_t* hdr_bytes = ToBytesUDPPacketHeader(pkt->header);
    if (hdr_bytes == NULL) return false;
    memcpy(out_frame, hdr_bytes, UDPPACKETHEADER_SIZE);
    free(hdr_bytes);

    /* payload */
    uint16_t payload_len = pkt->header->payload_size;
    if (payload_len > 0u && pkt->payload != NULL) {
        memcpy(out_frame + UDPPACKETHEADER_SIZE, pkt->payload, payload_len);
    }

    /* CRC32 over header + payload */
    uint16_t covered = (uint16_t)(UDPPACKETHEADER_SIZE + payload_len);
    rcsa_crc32_t ctx;
    rcsa_crc32_init(&ctx);
    rcsa_crc32_update(&ctx, out_frame, covered);
    uint32_t crc = rcsa_crc32_get(&ctx);
    out_frame[covered + 0] = (uint8_t)(crc >> 0);
    out_frame[covered + 1] = (uint8_t)(crc >> 8);
    out_frame[covered + 2] = (uint8_t)(crc >> 16);
    out_frame[covered + 3] = (uint8_t)(crc >> 24);

    return true;
}

/* ── public API ──────────────────────────────────────────────────────────── */

bool app_create_image_info_frame(uint8_t* out_frame)
{
    if (out_frame == NULL) return false;

    image_info_t info;
    info.width      = IMAGE_WIDTH;
    info.height     = IMAGE_HEIGHT;
    info.total_size = (uint32_t)IMAGE_WIDTH * IMAGE_HEIGHT;
    info.num_chunks = IMAGE_NUM_CHUNKS;

    uint16_t payload_len = (uint16_t)sizeof(image_info_t);

    UDPPacketHeader* hdr = CreateUDPPacketHeader(DEVICE_ID,
                                                  (uint8_t)CMD_GET_IMAGE_INFO,
                                                  payload_len);
    if (hdr == NULL) return false;

    UDPPacket* pkt = CreateUDPPacket(hdr);
    if (pkt == NULL) { free(hdr); return false; }

    AttachCompletedPayload(pkt, (const uint8_t*)&info, payload_len);

    bool ok = serialize_packet(pkt, out_frame);
    FreeUDPPacket(pkt);
    return ok;
}

bool app_create_chunk_frame(uint16_t chunk_idx, uint8_t* out_frame)
{
    if (out_frame == NULL) return false;

    uint16_t payload_len = (uint16_t)sizeof(chunk_data_t);

    UDPPacketHeader* hdr = CreateUDPPacketHeader(DEVICE_ID,
                                                  (uint8_t)CMD_GET_CHUNK,
                                                  payload_len);
    if (hdr == NULL) return false;

    UDPPacket* pkt = CreateUDPPacket(hdr);
    if (pkt == NULL) { free(hdr); return false; }

    /* เติมข้อมูลตรงเข้า pkt->payload ที่ malloc ไว้แล้ว — ไม่ต้องสร้าง chunk_data_t บน stack */
    uint16_t* idx_ptr  = (uint16_t*)pkt->payload;
    uint8_t*  data_ptr = pkt->payload + sizeof(uint16_t);
    *idx_ptr = chunk_idx;
    uint32_t base = (uint32_t)chunk_idx * IMAGE_CHUNK_SIZE;
    for (uint16_t i = 0; i < IMAGE_CHUNK_SIZE; i++) {
        data_ptr[i] = (uint8_t)((base + i) & 0xFFu);
    }
    pkt->payload_tail_index = payload_len;

    bool ok = serialize_packet(pkt, out_frame);
    FreeUDPPacket(pkt);
    return ok;
}
