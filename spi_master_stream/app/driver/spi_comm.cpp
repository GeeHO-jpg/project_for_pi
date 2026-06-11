#include "spi_comm.h"
#include "Packet_RCSA/SerialComm.h"
#include "Packet_RCSA/IDNumber.h"
#include "ring_buffer/ring_buffer.h"

// แต่ละ tick ส่ง/รับ exactly 1 packet = SPI_COMM_BUF_SIZE ไบต์ จึง 2x ก็เพียงพอ
// (16x ของเดิมจะกลายเป็น ~52KB/บัฟเฟอร์ เมื่อ BUF_SIZE ใหญ่ขึ้นมากใน big_chunk)
static constexpr uint16_t RB_SIZE = (uint16_t)(2u * SPI_COMM_BUF_SIZE + 1u);

static RingBuffer  g_tx_rb;
static RingBuffer  g_rx_rb;
static SerialComm* g_serial_tx;
static SerialComm* g_serial_rx;

static bool tx_read_dummy(uint8_t*)       { return false; }
static void tx_write_byte(uint8_t b)      { rb_put_byte(&g_tx_rb, b); }
static bool rx_read_byte(uint8_t* b)      { return rb_get_byte(&g_rx_rb, b); }
static void rx_write_dummy(uint8_t)       {}

void spi_comm_init(void) {
    InitializeIDNumber();
    SetIDNumber(1);
    rb_init(&g_tx_rb, RB_SIZE);
    rb_init(&g_rx_rb, RB_SIZE);
    g_serial_tx = CreateSerialComm(tx_read_dummy, tx_write_byte);
    g_serial_rx = CreateSerialComm(rx_read_byte,  rx_write_dummy);
}

void spi_comm_build_tx(uint8_t cmd, const uint8_t *payload, uint16_t len) {
    UDPPacketHeader* hdr = CreateUDPPacketHeader(1, cmd, len);
    UDPPacket*       pkt = CreateUDPPacket(hdr);
    AttachCompletedPayload(pkt, payload, len);
    SendUDPPacketSerialComm(g_serial_tx, pkt);
    FreeUDPPacket(pkt);
}

uint16_t spi_comm_drain_tx(uint8_t *buf, uint16_t buf_size) {
    return rb_get(&g_tx_rb, buf, buf_size);
}

void spi_comm_push_rx(const uint8_t *buf, uint16_t len) {
    rb_put(&g_rx_rb, buf, len);
}

UDPPacket* spi_comm_parse_rx(void) {
    RunReceiveSerialComm(g_serial_rx);
    return GetCompletePacketSerialComm(g_serial_rx);
}

void spi_comm_flush_rx(void) {
    rb_flush(&g_rx_rb);
    FreeIncompletePacketSerialComm(g_serial_rx);
    FreeCompletePacketSerialComm(g_serial_rx);
    ResetCircularBuffer(g_serial_rx->recv_buffer);
}
