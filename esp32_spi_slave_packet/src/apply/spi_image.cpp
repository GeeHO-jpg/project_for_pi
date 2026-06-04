#include "spi_image.h"
#include "app_create_pack.h"
#include "app_spi_rb.h"

bool spi_image_init(void)
{
    return spi_rb_init();
}

bool spi_image_push_info(void)
{
    uint8_t frame[SPI_FRAME_SIZE];
    if (!app_create_image_info_frame(frame)) return false;
    return spi_rb_push(frame);
}

bool spi_image_push_chunk(uint16_t chunk_idx)
{
    uint8_t frame[SPI_FRAME_SIZE];
    if (!app_create_chunk_frame(chunk_idx, frame)) return false;
    return spi_rb_push(frame);
}
