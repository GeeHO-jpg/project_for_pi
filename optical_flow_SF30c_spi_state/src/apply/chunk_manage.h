#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint16_t total_chunks(uint16_t payload_size, uint16_t chunk_size);
uint16_t select_chunk(uint8_t* chunk_buf, const uint8_t *payload, uint16_t payload_size, uint16_t chunk_size);

// คัดลอก chunk ที่ chunk_index ออกจาก payload (ไม่มี state ภายใน, ใช้ตอบ request ที่ระบุ index มา)
uint16_t get_chunk(uint8_t* chunk_buf, const uint8_t *payload, uint16_t payload_size, uint16_t chunk_size, uint16_t chunk_index);

#ifdef __cplusplus
}
#endif