#include "app_spi_rb.h"

#define SPI_TX_RB_SIZE  (SPI_FRAME_SIZE * 3u + 1u)

RingBuffer spi_tx_rb;

bool spi_rb_init(void)
{
    return rb_init(&spi_tx_rb, SPI_TX_RB_SIZE) == 0;
}

bool spi_rb_push(const uint8_t* frame)
{
    return rb_put_exact(&spi_tx_rb, frame, SPI_FRAME_SIZE);
}

bool spi_rb_pull(uint8_t* out_buf)
{
    if (!spi_rb_has_frame()) return false;
    rb_get(&spi_tx_rb, out_buf, SPI_FRAME_SIZE);
    return true;
}

bool spi_rb_has_frame(void)
{
    return rb_used(&spi_tx_rb) >= SPI_FRAME_SIZE;
}
