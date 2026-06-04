#include <cstdio>
#include <cstring>

#include "hal/include/spi_bus.hpp"
#include "hal/include/gpio_ready.hpp"
#include "Packet_RCSA/SerialComm.h"

// Frame layout: [header 9B][payload 19B][CRC 4B] = 32 bytes
static constexpr size_t PAYLOAD_SIZE = 19;
static constexpr size_t CRC_SIZE     = sizeof(uint32_t);
static constexpr size_t BUF_SIZE     = UDPPACKETHEADER_SIZE + PAYLOAD_SIZE + CRC_SIZE; // 32

static uint8_t tx_buf[BUF_SIZE];
static uint8_t rx_buf[BUF_SIZE];

// ── TX SerialComm ──
static SerialComm* g_serial_tx;
static size_t      tx_buf_idx;

static bool spi_tx_read_dummy(uint8_t*) { return false; }
static void spi_tx_write_byte(uint8_t b) {
    if (tx_buf_idx < BUF_SIZE) tx_buf[tx_buf_idx++] = b;
}

// ── RX SerialComm ──
static SerialComm* g_serial_rx;
static size_t      rx_parse_idx;

static bool spi_rx_read_byte(uint8_t* b) {
    if (rx_parse_idx < BUF_SIZE) {
        *b = rx_buf[rx_parse_idx++];
        return true;
    }
    return false;
}
static void spi_rx_write_dummy(uint8_t) {}

int main()
{
    InitializeIDNumber();
    SetIDNumber(1);

    hal::SPIBus   spi("/dev/spidev0.0", 1000000);
    hal::GPIOReady ready("gpiochip4", 22);   // Pi5: gpiochip4, GPIO 22 = Active HIGH

    g_serial_tx = CreateSerialComm(spi_tx_read_dummy, spi_tx_write_byte);
    g_serial_rx = CreateSerialComm(spi_rx_read_byte,  spi_rx_write_dummy);

    static const uint8_t payload_data[PAYLOAD_SIZE] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13
    };

    while (true) {
        // ── รอ ESP32 signal ready (blocking, rising edge) ──
        ready.waitReady();

        // ── Build TX packet ──
        UDPPacketHeader* hdr = CreateUDPPacketHeader(1, CMD_HANDSHAKE, PAYLOAD_SIZE);
        UDPPacket* tx_pkt = CreateUDPPacket(hdr);
        AttachCompletedPayload(tx_pkt, payload_data, PAYLOAD_SIZE);

        tx_buf_idx = 0;
        memset(tx_buf, 0, BUF_SIZE);
        SendUDPPacketSerialComm(g_serial_tx, tx_pkt);
        FreeUDPPacket(tx_pkt);

        // ── SPI transfer (full duplex) ──
        spi.transfer(tx_buf, rx_buf, BUF_SIZE);

        // ── Parse RX ──
        rx_parse_idx = 0;
        RunReceiveSerialComm(g_serial_rx);

        UDPPacket* rx_pkt = GetCompletePacketSerialComm(g_serial_rx);

        // ── Print ──
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

    FreeSerialComm(g_serial_tx);
    FreeSerialComm(g_serial_rx);
    return 0;
}
