#pragma once
#include <cstdint>
#include "driver/Packet_RCSA/UDPPacket.h"

/*
Protocol note:
- CMD_INFO is a sync/config step requested at startup and after resync.
- Normal streaming stays in DataRequest and cycles CMD_DATA index 0..last.
- CMD_DATA index 0 starts a new frame on the ESP32 side and selects a fresh snapshot.
- SPI is full-duplex with a one-transaction response delay, so the state machine
  tracks both the next requested index and the expected response index.
*/

enum class CommCmd : uint8_t {
    Info = 0x01,
    Data = 0x02,
};

// state ของ master state machine:
//   INFO_REQUEST ใช้ตอน startup/resync เพื่อ sync config
//   DATA_REQUEST ใช้ stream ปกติแบบวน CMD_DATA index 0..last ต่อเนื่อง
enum class CommState : uint8_t {
    InfoRequest, // กำลังขอ CMD_INFO (chunk_size/total_chunks ของ CMD_DATA)
    DataRequest, // กำลังขอ CMD_DATA ทีละ chunk ตาม chunk_index
};

/*
loop (วนตลอด)

  state = INFO_REQUEST
    PREPARE → ส่ง CMD_INFO (payload ว่าง)
    PARSE   → ถ้า rx เป็นคำตอบ CMD_INFO ที่ถูกต้อง
                 (payload[0:1]=chunk_size, payload[2:3]=total_chunks, payload[4:5]=last_chunk_size, ทั้งหมด uint16_t little-endian)
                 → clamp ไม่ให้เกิน data_capacity ที่ alloc ไว้ → state = DATA_REQUEST, chunk_index = 0
              ถ้าไม่ใช่ → ไม่ทำอะไร (tick ถัดไปจะส่ง CMD_INFO ซ้ำโดยอัตโนมัติ)

  state = DATA_REQUEST
    PREPARE → ส่ง CMD_DATA พร้อม chunk_index ที่ต้องการ (payload[0:1] = chunk_index, uint16_t little-endian)
    PARSE   → ถ้า rx เป็นคำตอบ CMD_DATA ที่ถูกต้อง (payload[0:1] = chunk_index ที่ slave echo กลับมา)
                 → เก็บข้อมูลลง buffer ที่ offset = echoed_index * chunk_size
                   (chunk สุดท้ายคัดลอกแค่ last_chunk_size ไบต์)
                 → ขอ chunk_index ถัดไป (เฉพาะถ้า echoed_index ไม่ใช่ index เก่าที่เคยรับแล้ว)
                  → ครบทุก chunk แล้ว → commit เป็นก้อนข้อมูลพร้อมใช้ (ขนาด = (total_chunks-1)*chunk_size + last_chunk_size)
                   → อยู่ใน state = DATA_REQUEST ต่อ และวนไปขอ CMD_DATA index=0 เพื่อเริ่ม snapshot/frame ถัดไป
              ถ้าไม่ใช่ → ไม่ทำอะไร (tick ถัดไปจะส่ง CMD_DATA(chunk_index เดิม) ซ้ำโดยอัตโนมัติ)

หมายเหตุ - 1-tick pipeline delay:
  SPI เป็น full-duplex "ping-pong": tx ที่ส่งออกในรอบนี้ จะได้ rx ตอบกลับใน "รอบถัดไป"
  ดังนั้น rx ที่ parse ในรอบนี้ อาจเป็นคำตอบของ tx ที่ "คนละ state" กับ state ปัจจุบัน
  (เช่น เพิ่งเปลี่ยนจาก INFO_REQUEST → DATA_REQUEST แต่ rx รอบนี้ยังเป็นคำตอบของ CMD_INFO อยู่)

  เรา track expected response index แยกจาก next request index เพราะ response ของ CMD_DATA จะตามหลัง request 1 transaction
  ถ้า cmd/index ไม่ตรง จะไม่ commit chunk นั้น และ stall watchdog จะพากลับไป CMD_INFO เมื่อไม่มี progress นานเกิน threshold
*/

// เริ่มต้น state machine
//   data_capacity = ขนาด buffer สูงสุด (byte) ที่ alloc ไว้รองรับข้อมูล CMD_DATA
//                   ต้อง >= chunk_size * total_chunks ที่ slave รายงานมาทาง CMD_INFO
//                   (ค่าที่เกินจะถูก clamp ลงมาที่ capacity นี้)
void app_state_init(uint32_t data_capacity);

// PREPARE: เตรียม tx packet ตัวถัดไปตาม state ปัจจุบัน (เรียก spi_comm_build_tx ภายใน)
void app_state_prepare_tx();

// PARSE: ป้อน packet ที่ parse ได้จาก rx (nullptr = parse ไม่ผ่าน/CRC หรือ signature ผิด) เข้า state machine
// คืนค่า true เมื่อ "ก้อนข้อมูล CMD_DATA ไว้ใช้งาน" ถูก commit ใหม่ในรอบนี้ (รับครบทุก chunk แล้ว)
bool app_state_handle_rx(UDPPacket* pkt);

// state ปัจจุบันของ state machine (สำหรับ debug/print)
CommState app_state_get(void);

// chunk_index ที่กำลังจะขอ/รออยู่ในรอบนี้ (มีผลเฉพาะตอน state == DataRequest, สำหรับ debug/print)
uint16_t app_state_get_next_index(void);

// ค่า chunk_size/total_chunks ล่าสุดที่ slave รายงานมาทาง CMD_INFO (0 ถ้ายังไม่เคยได้รับ)
void app_state_get_info(uint16_t* out_chunk_size, uint16_t* out_total_chunks);

// ก้อนข้อมูล CMD_DATA ล่าสุดที่ commit ไว้ใช้งาน, คืน nullptr ถ้ายังไม่เคยรับครบสักรอบ
// out_size = ขนาดข้อมูลจริง = (total_chunks-1)*chunk_size + last_chunk_size
const uint8_t* app_state_get_ready_data(uint32_t* out_size);

// Number of stall watchdog resyncs since startup.
uint32_t app_state_get_resync_count(void);

struct AppStateDebugCounters {
    uint32_t tx_info;
    uint32_t tx_data;
    uint32_t tx_frame_start;
    uint32_t no_pkt;
    uint32_t wrong_cmd;
    uint32_t bad_payload;
    uint32_t wrong_index;
    uint32_t incomplete_frames;
    uint32_t commits;
    uint16_t last_expected_index;
    uint16_t last_got_index;
    uint16_t last_payload_size;
};

void app_state_get_debug_counters(AppStateDebugCounters* out);
