#include "app_state.h"
#include "driver/spi_comm.h"

#include <cstdlib>
#include <cstring>

namespace {

struct Ctx {
    CommState state = CommState::InfoRequest;

    uint16_t data_capacity = 0;

    // ── ผลลัพธ์ล่าสุดจาก CMD_INFO ──
    uint8_t data_chunk_size   = 0;
    uint8_t data_total_chunks = 0;

    // ── chunk_index ที่กำลังขอ/รออยู่ (ใช้เฉพาะ state DataRequest) ──
    uint8_t next_index = 0;

    uint8_t* data_assembled = nullptr; // กำลังประกอบ
    uint8_t* data_ready     = nullptr; // commit แล้ว, พร้อมใช้งาน
    bool     data_ready_valid = false;
};

Ctx g_ctx;

} // namespace

void app_state_init(uint16_t data_capacity) {
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
            // CMD_DATA: ขอ chunk ที่ next_index, ฝัง index ลง byte แรกของ payload
            payload[0] = g_ctx.next_index;
            spi_comm_build_tx(static_cast<uint8_t>(CommCmd::Data), payload, sizeof(payload));
            break;
    }
}

bool app_state_handle_rx(UDPPacket* pkt) {
    bool committed = false;

    switch (g_ctx.state) {

        case CommState::InfoRequest: {
            if (!pkt || pkt->header->cmd != static_cast<uint8_t>(CommCmd::Info)
                     || pkt->header->payload_size < 2) {
                break; // cmd ไม่ตรง/packet เสีย -> tick ถัดไปส่ง CMD_INFO ซ้ำเอง
            }

            uint8_t chunk_size   = pkt->payload[0];
            uint8_t total_chunks = pkt->payload[1];

            // clamp ไม่ให้เกิน buffer capacity ที่ alloc ไว้ตอน app_state_init
            if (chunk_size == 0) chunk_size = 1;
            uint16_t max_total = (uint16_t)(g_ctx.data_capacity / chunk_size);
            if (max_total == 0) max_total = 1;
            if (total_chunks == 0 || total_chunks > max_total) {
                total_chunks = (uint8_t)max_total;
            }

            g_ctx.data_chunk_size   = chunk_size;
            g_ctx.data_total_chunks = total_chunks;

            // ต่อไปขอ CMD_DATA ตั้งแต่ chunk แรก
            g_ctx.next_index = 0;
            g_ctx.state      = CommState::DataRequest;
            break;
        }

        case CommState::DataRequest: {
            if (!pkt || pkt->header->cmd != static_cast<uint8_t>(CommCmd::Data)
                     || pkt->header->payload_size < 1) {
                break; // cmd ไม่ตรง/packet เสีย -> tick ถัดไปส่ง CMD_DATA(next_index) ซ้ำเอง
            }

            uint8_t echoed_index = pkt->payload[0];
            if (echoed_index != g_ctx.next_index) {
                break; // คำตอบของ chunk เก่าที่เคยรับไปแล้ว (duplicate จาก pipeline delay) -> ข้าม
            }

            uint16_t copy_len = pkt->header->payload_size - 1;
            if (copy_len > g_ctx.data_chunk_size) copy_len = g_ctx.data_chunk_size;

            uint16_t offset = (uint16_t)echoed_index * g_ctx.data_chunk_size;
            memcpy(g_ctx.data_assembled + offset, &pkt->payload[1], copy_len);

            g_ctx.next_index++;
            if (g_ctx.next_index < g_ctx.data_total_chunks) {
                break; // ยังไม่ครบ -> tick ถัดไปขอ chunk ถัดไปต่อ
            }

            // ครบทุก chunk แล้ว -> commit เป็นก้อนข้อมูลพร้อมใช้ แล้ววนกลับไปขอ CMD_INFO
            uint16_t total_size = (uint16_t)g_ctx.data_total_chunks * g_ctx.data_chunk_size;
            memcpy(g_ctx.data_ready, g_ctx.data_assembled, total_size);
            g_ctx.data_ready_valid = true;
            committed = true;

            g_ctx.next_index = 0;
            g_ctx.state      = CommState::InfoRequest;
            break;
        }
    }

    return committed;
}

CommState app_state_get(void) {
    return g_ctx.state;
}

void app_state_get_info(uint8_t* out_chunk_size, uint8_t* out_total_chunks) {
    if (out_chunk_size)   *out_chunk_size   = g_ctx.data_chunk_size;
    if (out_total_chunks) *out_total_chunks = g_ctx.data_total_chunks;
}

const uint8_t* app_state_get_ready_data(uint16_t* out_size) {
    if (out_size) *out_size = (uint16_t)g_ctx.data_total_chunks * g_ctx.data_chunk_size;
    return g_ctx.data_ready_valid ? g_ctx.data_ready : nullptr;
}
