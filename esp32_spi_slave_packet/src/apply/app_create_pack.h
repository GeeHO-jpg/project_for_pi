#pragma once

#include <stdint.h>
#include <stdbool.h>

/* hdr(9) + max_payload chunk_data_t(514) + crc(4) + pad(1) = 528, multiple of 4 */
#define SPI_FRAME_SIZE  528u

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Serialize an image-info packet into out_frame[SPI_FRAME_SIZE].
 * Uses Packet_RCSA: CreateUDPPacketHeader → CreateUDPPacket →
 *                   AttachCompletedPayload → ToBytesUDPPacketHeader → CRC32
 */
bool app_create_image_info_frame(uint8_t* out_frame);

/*
 * Serialize a chunk packet for chunk_idx into out_frame[SPI_FRAME_SIZE].
 * Same pipeline as above.
 */
bool app_create_chunk_frame(uint16_t chunk_idx, uint8_t* out_frame);

#ifdef __cplusplus
}
#endif
