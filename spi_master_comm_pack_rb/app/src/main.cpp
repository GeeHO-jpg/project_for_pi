#include <cstdio>
#include <cstring>

#include "hal/include/spi_bus.hpp"
#include "hal/include/gpio_ready.hpp"
#include "Packet_RCSA/SerialComm.h"
#include "ring_buffer/ring_buffer.h"

// Frame layout: [header 9B][payload 19B][CRC 4B] = 32 bytes
static constexpr size_t PAYLOAD_SIZE = 19;
static constexpr size_t CRC_SIZE     = sizeof(uint32_t);
static constexpr size_t BUF_SIZE     = UDPPACKETHEADER_SIZE + PAYLOAD_SIZE + CRC_SIZE; // 32

// ring buffer: 16 packets depth (+1 for one-slot-wasted convention)
static constexpr uint16_t RB_SIZE = (uint16_t)(16 * BUF_SIZE + 1);

static RingBuffer g_tx_rb;   // serialized TX bytes รอส่ง SPI
static RingBuffer g_rx_rb;   // raw RX bytes รอ parse

static uint8_t tx_buf[BUF_SIZE];
static uint8_t rx_buf[BUF_SIZE];

// ── TX SerialComm: push to ring buffer ──────────────────────────────────────
static SerialComm* g_serial_tx;

static bool spi_tx_read_dummy(uint8_t*) { return false; }
static void spi_tx_write_byte(uint8_t b) { rb_put_byte(&g_tx_rb, b); }

// ── RX SerialComm: pop from ring buffer ─────────────────────────────────────
static SerialComm* g_serial_rx;

static bool spi_rx_read_byte(uint8_t* b) { return rb_get_byte(&g_rx_rb, b); }
static void spi_rx_write_dummy(uint8_t) {}

int main()
{
    InitializeIDNumber();
    SetIDNumber(1);

    rb_init(&g_tx_rb, RB_SIZE);
    rb_init(&g_rx_rb, RB_SIZE);

    hal::SPIBus    spi("/dev/spidev0.0", 1000000);
    hal::GPIOReady ready("gpiochip4", 22);

    g_serial_tx = CreateSerialComm(spi_tx_read_dummy, spi_tx_write_byte);
    g_serial_rx = CreateSerialComm(spi_rx_read_byte,  spi_rx_write_dummy);

    static const uint8_t payload_data[PAYLOAD_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13
    };

    while (true) {
        // ── Build TX packet → push to TX ring buffer ──────────────────────
        UDPPacketHeader* hdr = CreateUDPPacketHeader(1, CMD_HANDSHAKE, PAYLOAD_SIZE);
        UDPPacket* tx_pkt = CreateUDPPacket(hdr);
        AttachCompletedPayload(tx_pkt, payload_data, PAYLOAD_SIZE);
        SendUDPPacketSerialComm(g_serial_tx, tx_pkt);   // → spi_tx_write_byte → g_tx_rb
        FreeUDPPacket(tx_pkt);

        // ── Drain TX ring buffer → tx_buf[] ──────────────────────────────
        memset(tx_buf, 0, BUF_SIZE);
        rb_get(&g_tx_rb, tx_buf, BUF_SIZE);

        // ── รอ ESP32 signal ready ─────────────────────────────────────────
        ready.waitReady();

        // ── SPI transfer (full duplex) ────────────────────────────────────
        spi.transfer(tx_buf, rx_buf, BUF_SIZE);

        // ── Push RX bytes → RX ring buffer ───────────────────────────────
        rb_put(&g_rx_rb, rx_buf, BUF_SIZE);

        // ── Parse จาก RX ring buffer ─────────────────────────────────────
        RunReceiveSerialComm(g_serial_rx);              // ← spi_rx_read_byte ← g_rx_rb
        UDPPacket* rx_pkt = GetCompletePacketSerialComm(g_serial_rx);

        // ── Print ─────────────────────────────────────────────────────────
        printf("[TX]:");
        for (size_t i = 0; i < BUF_SIZE; i++) printf(" %02X", tx_buf[i]);

        printf("\n[RX]:");
        for (size_t i = 0; i < BUF_SIZE; i++) printf(" %02X", rx_buf[i]);

        if (rx_pkt != NULL) {
            printf("\n[RX] OK  id=%u cmd=0x%02X\n\n",
                   rx_pkt->header->id, rx_pkt->header->cmd);
            FreeUDPPacket(rx_pkt);
        } else {
            printf("\n[RX] FAIL (bad signature or CRC)\n\n");
        }
    }

    rb_free(&g_tx_rb);
    rb_free(&g_rx_rb);
    FreeSerialComm(g_serial_tx);
    FreeSerialComm(g_serial_rx);
    return 0;
}
