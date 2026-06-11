#pragma once
#include <cstdint>
#include "driver/Packet_RCSA/UDPPacket.h"

enum class CommCmd : uint8_t {
    Info = 0x01,
    Data = 0x02,
};

// state ของ master state machine — วนเป็นวงกลม: INFO_REQUEST -> DATA_REQUEST -> INFO_REQUEST -> ...
enum class CommState : uint8_t {
    InfoRequest, // กำลังขอ CMD_INFO (chunk_size/total_chunks ของ CMD_DATA)
    DataRequest, // กำลังขอ CMD_DATA ทีละ chunk ตาม chunk_index
};

/*
loop (วนตลอด)

  state = INFO_REQUEST
    PREPARE → ส่ง CMD_INFO (payload ว่าง)
    PARSE   → ถ้า rx เป็นคำตอบ CMD_INFO ที่ถูกต้อง (payload[0]=chunk_size, payload[1]=total_chunks)
                 → clamp ไม่ให้เกิน data_capacity ที่ alloc ไว้ → state = DATA_REQUEST, chunk_index = 0
              ถ้าไม่ใช่ → ไม่ทำอะไร (tick ถัดไปจะส่ง CMD_INFO ซ้ำโดยอัตโนมัติ)

  state = DATA_REQUEST
    PREPARE → ส่ง CMD_DATA พร้อม chunk_index ที่ต้องการ (payload[0] = chunk_index)
    PARSE   → ถ้า rx เป็นคำตอบ CMD_DATA ที่ถูกต้อง (payload[0] = chunk_index ที่ slave echo กลับมา)
                 → เก็บข้อมูลลง buffer ที่ offset = echoed_index * chunk_size
                 → ขอ chunk_index ถัดไป (เฉพาะถ้า echoed_index ไม่ใช่ index เก่าที่เคยรับแล้ว)
                 → ครบทุก chunk แล้ว → commit เป็นก้อนข้อมูลพร้อมใช้ → state = INFO_REQUEST, chunk_index = 0
              ถ้าไม่ใช่ → ไม่ทำอะไร (tick ถัดไปจะส่ง CMD_DATA(chunk_index เดิม) ซ้ำโดยอัตโนมัติ)

หมายเหตุ - 1-tick pipeline delay:
  SPI เป็น full-duplex "ping-pong": tx ที่ส่งออกในรอบนี้ จะได้ rx ตอบกลับใน "รอบถัดไป"
  ดังนั้น rx ที่ parse ในรอบนี้ อาจเป็นคำตอบของ tx ที่ "คนละ state" กับ state ปัจจุบัน
  (เช่น เพิ่งเปลี่ยนจาก INFO_REQUEST → DATA_REQUEST แต่ rx รอบนี้ยังเป็นคำตอบของ CMD_INFO อยู่)

  เราไม่ต้อง track เรื่องนี้เป็นพิเศษ เพราะ PARSE จะเช็ค cmd ของ rx ให้ตรงกับ state ปัจจุบันเสมอ
  ถ้าไม่ตรง (cmd คนละแบบ, หรือ chunk_index ที่ echo มาเป็นตัวที่เคยรับแล้ว) ก็แค่ "ไม่ทำอะไร"
  แล้ว PREPARE รอบถัดไปจะส่งคำขอเดิมซ้ำเอง — ระบบจะ sync ตัวเองเข้าที่ภายในไม่กี่ tick
*/

// เริ่มต้น state machine
//   data_capacity = ขนาด buffer สูงสุด (byte) ที่ alloc ไว้รองรับข้อมูล CMD_DATA
//                   ต้อง >= chunk_size * total_chunks ที่ slave รายงานมาทาง CMD_INFO
//                   (ค่าที่เกินจะถูก clamp ลงมาที่ capacity นี้)
void app_state_init(uint16_t data_capacity);

// PREPARE: เตรียม tx packet ตัวถัดไปตาม state ปัจจุบัน (เรียก spi_comm_build_tx ภายใน)
void app_state_prepare_tx();

// PARSE: ป้อน packet ที่ parse ได้จาก rx (nullptr = parse ไม่ผ่าน/CRC หรือ signature ผิด) เข้า state machine
// คืนค่า true เมื่อ "ก้อนข้อมูล CMD_DATA ไว้ใช้งาน" ถูก commit ใหม่ในรอบนี้ (รับครบทุก chunk แล้ว)
bool app_state_handle_rx(UDPPacket* pkt);

// state ปัจจุบันของ state machine (สำหรับ debug/print)
CommState app_state_get(void);

// ค่า chunk_size/total_chunks ล่าสุดที่ slave รายงานมาทาง CMD_INFO (0 ถ้ายังไม่เคยได้รับ)
void app_state_get_info(uint8_t* out_chunk_size, uint8_t* out_total_chunks);

// ก้อนข้อมูล CMD_DATA ล่าสุดที่ commit ไว้ใช้งาน, คืน nullptr ถ้ายังไม่เคยรับครบสักรอบ
const uint8_t* app_state_get_ready_data(uint16_t* out_size);
