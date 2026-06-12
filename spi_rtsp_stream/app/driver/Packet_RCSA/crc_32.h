/*
 * crc_32.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_CRC_32_H_
#define INC_PACKET_HEADER_CRC_32_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t crc;
} rcsa_crc32_t;

void rcsa_crc32_init(rcsa_crc32_t *ctx);
void rcsa_crc32_reset(rcsa_crc32_t *ctx);
bool rcsa_crc32_update(rcsa_crc32_t *ctx, const unsigned char *data, size_t data_size);
uint32_t rcsa_crc32_get(const rcsa_crc32_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* INC_PACKET_HEADER_CRC_32_H_ */
