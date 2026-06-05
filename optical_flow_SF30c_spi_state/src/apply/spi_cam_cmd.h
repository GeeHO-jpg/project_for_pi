#pragma once

#include <stdint.h>

/*
 * SPI camera protocol command IDs — slave side (optical flow ESP32-S3).
 *
 * Values must stay in sync with the master's CommandType enum
 * (spi_cam_master_state/app/driver/Packet_RCSA/common_defs.h).
 * Master assigns these immediately after CMD_CAMERA_CODE (= 49).
 */
typedef enum {
    SPI_CAM_CMD_NONE = 0,
    SPI_CAM_CMD_INFO,
    SPI_CAM_CMD_CHUNK,
} SpiCamCmd;
