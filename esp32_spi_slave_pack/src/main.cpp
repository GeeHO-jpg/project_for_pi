#include <Arduino.h>
#include <ESP32SPISlave.h>
#include "config.hpp"

#include "Packet_RCSA/SerialComm.h"

static ESP32SPISlave slave;

// Frame layout: [header 9B][payload 19B][CRC 4B] = 32 bytes
static constexpr size_t PAYLOAD_SIZE = 19;
static constexpr size_t CRC_SIZE     = sizeof(uint32_t);   // 4
static constexpr size_t BUF_SIZE     = UDPPACKETHEADER_SIZE + PAYLOAD_SIZE + CRC_SIZE; // 32

static uint8_t __attribute__((aligned(4))) tx_buf[BUF_SIZE];
static uint8_t __attribute__((aligned(4))) rx_dma[2][BUF_SIZE];

static QueueHandle_t g_print_queue;

// --- TX SerialComm: Write callback เขียนลง tx_buf ทีละ byte ---
static SerialComm* g_serial_tx;
static size_t      tx_buf_idx;

static bool spi_tx_read_dummy(uint8_t*) { return false; }
static void spi_tx_write_byte(uint8_t b) {
    if (tx_buf_idx < BUF_SIZE) tx_buf[tx_buf_idx++] = b;
}

// --- RX SerialComm: Read callback อ่านจาก rx_dma ทีละ byte ---
static SerialComm* g_serial_rx;
static uint8_t*    rx_parse_buf;   // ชี้ไปยัง rx_dma[buf_idx] ที่กำลัง parse
static size_t      rx_parse_idx;

static bool spi_rx_read_byte(uint8_t* b) {
    if (rx_parse_idx < BUF_SIZE) {
        *b = rx_parse_buf[rx_parse_idx++];
        return true;
    }
    return false;
}
static void spi_rx_write_dummy(uint8_t) {}

struct PrintMsg {
    uint8_t  tx[BUF_SIZE];
    uint8_t  rx_raw[BUF_SIZE];
    size_t   rx_len;
    bool     rx_pkt_ok;       // CRC ผ่านไหม
    uint16_t rx_pkt_id;
    uint8_t  rx_pkt_cmd;
};

static void task_spi(void* arg);
static void task_print(void* arg);

void setup() {
    Serial.begin(115200);

    InitializeIDNumber();
    SetIDNumber(1);

    slave.setDataMode(SPI_MODE0);
    slave.begin(FSPI, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    memset(tx_buf, 0, sizeof(tx_buf));
    memset(rx_dma, 0, sizeof(rx_dma));

    g_serial_tx = CreateSerialComm(spi_tx_read_dummy, spi_tx_write_byte);
    g_serial_rx = CreateSerialComm(spi_rx_read_byte,  spi_rx_write_dummy);

    g_print_queue = xQueueCreate(4, sizeof(PrintMsg));

    xTaskCreatePinnedToCore(task_spi,   "spi",   4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(task_print, "print", 4096, nullptr, 1, nullptr, 1);
}

void loop() {}

static void task_spi(void* arg) {
    int buf_idx = 0;
    PrintMsg msg;

    static const uint8_t payload_data[PAYLOAD_SIZE] = {
        0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
        0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
        0xEF, 0xEE, 0xED
    };

    while (true) {
        // ── TX: build packet → serialize → tx_buf ──────────────────────
        UDPPacketHeader* hdr = CreateUDPPacketHeader(1, 0x01, PAYLOAD_SIZE);
        UDPPacket* pkt = CreateUDPPacket(hdr);
        AttachCompletedPayload(pkt, payload_data, PAYLOAD_SIZE);

        tx_buf_idx = 0;
        SendUDPPacketSerialComm(g_serial_tx, pkt);
        FreeUDPPacket(pkt);

        // ── SPI transfer (blocking) ─────────────────────────────────────
        size_t received = slave.transfer(tx_buf, rx_dma[buf_idx], BUF_SIZE);

        // ── RX: parse rx_dma ผ่าน SerialComm (validate signature + CRC) ─
        rx_parse_buf = rx_dma[buf_idx];
        rx_parse_idx = 0;
        RunReceiveSerialComm(g_serial_rx);   // อ่านผ่าน spi_rx_read_byte แล้ว parse + check CRC

        UDPPacket* rx_pkt = GetCompletePacketSerialComm(g_serial_rx);

        // ── ส่งผลไปให้ print task ───────────────────────────────────────
        memcpy(msg.tx,     tx_buf,           BUF_SIZE);
        memcpy(msg.rx_raw, rx_dma[buf_idx],  BUF_SIZE);
        msg.rx_len    = received;
        msg.rx_pkt_ok = (rx_pkt != NULL);

        if (rx_pkt != NULL) {
            msg.rx_pkt_id  = rx_pkt->header->id;
            msg.rx_pkt_cmd = rx_pkt->header->cmd;
            FreeUDPPacket(rx_pkt);
        }

        xQueueSend(g_print_queue, &msg, 0);

        buf_idx ^= 1;
    }
}

static void task_print(void* arg) {
    PrintMsg msg;
    while (true) {
        if (xQueueReceive(g_print_queue, &msg, portMAX_DELAY)) {
            printf("[TX]:");
            for (size_t i = 0; i < BUF_SIZE; i++) printf(" %02X", msg.tx[i]);

            printf("\n[RX]:");
            for (size_t i = 0; i < msg.rx_len; i++) printf(" %02X", msg.rx_raw[i]);

            if (msg.rx_pkt_ok)
                printf("\n[RX] OK  id=%u cmd=0x%02X\n", msg.rx_pkt_id, msg.rx_pkt_cmd);
            else
                printf("\n[RX] FAIL (bad signature or CRC)\n");
        }
    }
}
