#include "app_hal_test.h"
#include "hal/include/spi_bus.hpp"
#include "Packet_RCSA/UDPPacketHeader.h"
#include "Packet_RCSA/common_defs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

static hal::SPIBus* g_spi = nullptr;

void test_hal_spi_init(void)
{
    g_spi = new hal::SPIBus("/dev/spidev0.0", 1000000);
}

static void build_dummy_tx(uint8_t* tx)
{
    memset(tx, 0, SPI_FRAME_SIZE);
    UDPPacketHeader* hdr = CreateUDPPacketHeader(0, CMD_NONE, 0);
    if (!hdr) return;
    uint8_t* hdr_bytes = ToBytesUDPPacketHeader(hdr);
    if (hdr_bytes) {
        memcpy(tx, hdr_bytes, UDPPACKETHEADER_SIZE);
        free(hdr_bytes);
    }
    free(hdr);
}

void test_hal_spi_run(void)
{
    uint8_t tx[SPI_FRAME_SIZE];
    uint8_t rx[SPI_FRAME_SIZE];

    build_dummy_tx(tx);

    while (true) {
        g_spi->transfer(tx, rx, SPI_FRAME_SIZE);

        UDPPacketHeader* hdr = GetUDPPacketHeader(rx, SPI_FRAME_SIZE);
        if (hdr) {
            printf("[SPI] RX Header: sig=%.4s id=%u cmd=%u payload_size=%u\n",
                   hdr->signature, hdr->id, hdr->cmd, hdr->payload_size);
            free(hdr);
        } else {
            printf("[SPI] RX Header: invalid signature\n");
        }

        usleep(1000);
    }
}
