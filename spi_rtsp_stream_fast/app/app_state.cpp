#include "app_state.h"
#include "driver/spi_comm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// ── little-endian uint16 helpers (wire format) ──
inline uint16_t get_u16le(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
inline void put_u16le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

struct Ctx {
    CommState state = CommState::InfoRequest;

    uint32_t data_capacity = 0;

    // ── ผลลัพธ์ล่าสุดจาก CMD_INFO ──
    uint16_t data_chunk_size      = 0;
    uint16_t data_total_chunks    = 0;
    uint16_t data_last_chunk_size = 0;
    uint32_t data_payload_size    = 0; // = (total_chunks-1)*chunk_size + last_chunk_size

    // ── chunk_index ที่กำลังขอ/รออยู่ (ใช้เฉพาะ state DataRequest) ──
    uint16_t next_index = 0;

    uint8_t* data_assembled = nullptr; // กำลังประกอบ
    uint8_t* data_ready     = nullptr; // commit แล้ว, พร้อมใช้งาน
    bool     data_ready_valid = false;

    // จำนวน tick ติดต่อกันที่ RX FAIL/ไม่ตรงกับ request ปัจจุบัน (ใช้ตรวจจับ slave ค้าง)
    uint32_t stall_ticks = 0;
    uint32_t resync_count = 0;
    AppStateDebugCounters debug = {};
};

// ถ้า RX FAIL ติดต่อกันเกินจำนวนนี้ ถือว่า slave ค้าง/รีเซ็ตตัวเองไปแล้ว
// -> resync กลับไปเริ่มที่ CMD_INFO ใหม่ + ล้าง rx ring buffer ไม่ให้ค้างขอ chunk เดิมตลอดไป
constexpr uint32_t STALL_RESYNC_THRESHOLD = 30;

Ctx g_ctx;

} // namespace

void app_state_init(uint32_t data_capacity) {
    free(g_ctx.data_assembled);
    free(g_ctx.data_ready);

    g_ctx = Ctx{};
    g_ctx.data_capacity = data_capacity;

    g_ctx.data_assembled = (uint8_t*)malloc(data_capacity);
    g_ctx.data_ready     = (uint8_t*)malloc(data_capacity);
    memset(g_ctx.data_assembled, 0, data_capacity);
    memset(g_ctx.data_ready, 0, data_capacity);
}

void app_state_prepare_tx() {
    uint8_t payload[SPI_COMM_FRAME_PAYLOAD_SIZE] = {0};

    switch (g_ctx.state) {
        case CommState::InfoRequest:
            // CMD_INFO: ขอ chunk_size/total_chunks ของ CMD_DATA (payload ว่าง)
            spi_comm_build_tx(static_cast<uint8_t>(CommCmd::Info), payload, sizeof(payload));
            break;

        case CommState::DataRequest:
            // CMD_DATA: ขอ chunk ที่ next_index, ฝัง index ลง payload[0:1] (uint16_t little-endian)
            put_u16le(&payload[0], g_ctx.next_index);
            spi_comm_build_tx(static_cast<uint8_t>(CommCmd::Data), payload, sizeof(payload));
            break;
    }
}

bool app_state_handle_rx(UDPPacket* pkt) {
    bool committed  = false;
    bool progressed = false; // true ถ้า tick นี้ทำให้ state machine ขยับไปข้างหน้า (ไม่ใช่แค่ retry เดิม)

    switch (g_ctx.state) {

        case CommState::InfoRequest: {
            if (!pkt) {
                g_ctx.debug.no_pkt++;
                break;
            }
            if (pkt->header->cmd != static_cast<uint8_t>(CommCmd::Info)) {
                g_ctx.debug.wrong_cmd++;
                break;
            }
            if (pkt->header->payload_size < 6) {
                g_ctx.debug.bad_payload++;
                break; // cmd ไม่ตรง/packet เสีย -> tick ถัดไปส่ง CMD_INFO ซ้ำเอง
            }

            uint16_t chunk_size      = get_u16le(&pkt->payload[0]);
            uint16_t total_chunks    = get_u16le(&pkt->payload[2]);
            uint16_t last_chunk_size = get_u16le(&pkt->payload[4]);

            // clamp ไม่ให้เกิน buffer capacity ที่ alloc ไว้ตอน app_state_init
            if (chunk_size == 0) chunk_size = 1;
            uint32_t max_total32 = g_ctx.data_capacity / chunk_size;
            if (max_total32 == 0) max_total32 = 1;
            if (max_total32 > 0xFFFFu) max_total32 = 0xFFFFu;
            uint16_t max_total = (uint16_t)max_total32;
            if (total_chunks == 0 || total_chunks > max_total) {
                total_chunks = max_total;
            }

            // last_chunk_size ต้องอยู่ในช่วง 1..chunk_size, ถ้าไม่ถูกต้อง fallback เป็น chunk_size เต็ม
            if (last_chunk_size == 0 || last_chunk_size > chunk_size) {
                last_chunk_size = chunk_size;
            }

            g_ctx.data_chunk_size      = chunk_size;
            g_ctx.data_total_chunks    = total_chunks;
            g_ctx.data_last_chunk_size = last_chunk_size;
            g_ctx.data_payload_size    = (uint32_t)(total_chunks - 1) * chunk_size + last_chunk_size;

            // ต่อไปขอ CMD_DATA ตั้งแต่ chunk แรก
            g_ctx.next_index = 0;
            g_ctx.state      = CommState::DataRequest;
            progressed       = true;
            break;
        }

        case CommState::DataRequest: {
            if (!pkt) {
                g_ctx.debug.no_pkt++;
                break;
            }
            if (pkt->header->cmd != static_cast<uint8_t>(CommCmd::Data)) {
                g_ctx.debug.wrong_cmd++;
                break;
            }
            if (pkt->header->payload_size < 2) {
                g_ctx.debug.bad_payload++;
                break; // cmd ไม่ตรง/packet เสีย -> tick ถัดไปส่ง CMD_DATA(next_index) ซ้ำเอง
            }

            uint16_t echoed_index = get_u16le(&pkt->payload[0]);
            if (echoed_index != g_ctx.next_index) {
                g_ctx.debug.wrong_index++;
                g_ctx.debug.last_expected_index = g_ctx.next_index;
                g_ctx.debug.last_got_index = echoed_index;
                g_ctx.debug.last_payload_size = pkt->header->payload_size;
                break; // คำตอบของ chunk เก่าที่เคยรับไปแล้ว (duplicate จาก pipeline delay) -> ข้าม
            }

            uint16_t this_chunk_size = (echoed_index == (uint16_t)(g_ctx.data_total_chunks - 1))
                                            ? g_ctx.data_last_chunk_size
                                            : g_ctx.data_chunk_size;

            uint16_t copy_len = pkt->header->payload_size - 2;
            if (copy_len > this_chunk_size) copy_len = this_chunk_size;

            uint32_t offset = (uint32_t)echoed_index * g_ctx.data_chunk_size;
            memcpy(g_ctx.data_assembled + offset, &pkt->payload[2], copy_len);

            g_ctx.next_index++;
            progressed = true; // ได้ chunk ใหม่เพิ่ม -> ถือว่าขยับไปข้างหน้าแล้ว

            if (g_ctx.next_index < g_ctx.data_total_chunks) {
                break; // ยังไม่ครบ -> tick ถัดไปขอ chunk ถัดไปต่อ
            }

            // ครบทุก chunk แล้ว -> commit เป็นก้อนข้อมูลพร้อมใช้ แล้ววนกลับไปขอ CMD_INFO
            memcpy(g_ctx.data_ready, g_ctx.data_assembled, g_ctx.data_payload_size);
            g_ctx.data_ready_valid = true;
            committed = true;
            g_ctx.debug.commits++;

            g_ctx.next_index = 0;
            g_ctx.state      = CommState::InfoRequest;
            break;
        }
    }

    // ── stall watchdog: ถ้าไม่มีความคืบหน้าติดต่อกันนานเกินไป (slave ค้าง/รีเซ็ตตัวเอง) ──
    // resync กลับไปขอ CMD_INFO ใหม่ทั้งหมด + ล้าง rx ring buffer ที่อาจมีขยะตกค้างอยู่
    // ป้องกันไม่ให้ค้างขอ chunk เดิม (เช่น idx=17) วนไปตลอดจนต้องไปรีเซ็ตฝั่ง slave เอง
    if (progressed) {
        g_ctx.stall_ticks = 0;
    } else {
        g_ctx.stall_ticks++;
        if (g_ctx.stall_ticks >= STALL_RESYNC_THRESHOLD) {
            printf("[RESYNC] no progress for %u ticks (state=%s, next_index=%u) -> back to INFO_REQUEST + flush rx\n",
                   STALL_RESYNC_THRESHOLD,
                   (g_ctx.state == CommState::InfoRequest) ? "INFO_REQUEST" : "DATA_REQUEST",
                   g_ctx.next_index);
            g_ctx.stall_ticks = 0;
            g_ctx.resync_count++;
            g_ctx.next_index  = 0;
            g_ctx.state       = CommState::InfoRequest;
            spi_comm_flush_rx();
        }
    }

    return committed;
}

CommState app_state_get(void) {
    return g_ctx.state;
}

uint16_t app_state_get_next_index(void) {
    return g_ctx.next_index;
}

void app_state_get_info(uint16_t* out_chunk_size, uint16_t* out_total_chunks) {
    if (out_chunk_size)   *out_chunk_size   = g_ctx.data_chunk_size;
    if (out_total_chunks) *out_total_chunks = g_ctx.data_total_chunks;
}

const uint8_t* app_state_get_ready_data(uint32_t* out_size) {
    if (out_size) *out_size = g_ctx.data_payload_size;
    return g_ctx.data_ready_valid ? g_ctx.data_ready : nullptr;
}

uint32_t app_state_get_resync_count(void) {
    return g_ctx.resync_count;
}

void app_state_get_debug_counters(AppStateDebugCounters* out) {
    if (out) {
        *out = g_ctx.debug;
    }
}
