/*
 * common_defs.h
 *
 *  Created on: Mar 9, 2026
 *      Author: Lenovo
 */

#ifndef INC_COMMON_DEFS_H_
#define INC_COMMON_DEFS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SERIAL_NUMBER_LENGTH 64

typedef enum {
    RTK_STATUS_NOFIX = 0,
    RTK_STATUS_2DFIX,
    RTK_STATUS_DGPS,
    RTK_STATUS_3DFIX,
    RTK_STATUS_FLOAT,
    RTK_STATUS_FIX
} RCSA_RTK_STATUS;

typedef enum {
    FILE_START = 0,
    FILE_CHUNK,
    FILE_EOF
} RCSA_FILE;

typedef enum {
    MODE_STABILIZE = 0,
    MODE_LOITER,
    MODE_GUIDED,
    MODE_AUTO,
    MODE_RTL
} RCSA_MODE;

typedef enum {
    MISSION_UPLOAD = 0,
    MISSION_START,
    MISSION_STOP,
    MISSION_CLEAR,
    MISSION_STATUS
} RCSA_MISSION;

typedef enum {
    RCSA_UAV_DISARM = 0,
    RCSA_UAV_ARM
} RCSA_UAV;

typedef enum {
    RCSA_UAV_STATUS_OK = 0,
    RCSA_UAV_STATUS_NG
} RCSA_UAV_STATUS;

typedef enum {
    DOCK_IDLE = 0,
    DOCK_LANDING,
    DOCK_READY,
    DOCK_LAUNCHING,
    DOCK_ERROR
} DockState;

typedef enum {
    DOOR_CLOSE = 0,
    DOOR_OPEN,
    DOOR_MOVING
} DockDoorState;

typedef enum {
    CHARGE_OFF = 0,
    CHARGE_ON
} ChargingState;

typedef enum {
    CHECK_RX_SIZE   =   0x3A,
    READ_PAYLOAD    =   0xFA,
    CHECK_TX_FREE   =   0x5A,
    WRITE_PAYLOAD   =   0xA5
} SPIINTERFACE;

typedef enum {

    CMD_NONE = 0,

    CMD_HANDSHAKE,
    CMD_INFO,
    CMD_REBOOT,

    CMD_RTCM,

    CMD_ARM,
    CMD_DISARM,
    CMD_MODE,
    CMD_MISSION,
    CMD_TAKEOFF,
    CMD_LANDING,
    CMD_RTL,
    CMD_GOTO,
    CMD_MOVE,

    CMD_MISSION_REQUEST,
    CMD_MISSION_NEXT_POS,
    CMD_MISSION_OLD_POS,

    CMD_UPLOADFILE,
    CMD_FILE_REQUEST_NEXT,
    CMD_FILE_REQUEST_REPEAT,
    CMD_FILE,
    CMD_FILEEOF,

    ROI_LOCK,
    ROI_CANCEL,
    ROI_ACK,

    CMD_CAM_GSP_SET,
    CMD_CAM_GSP_UP_INDEX,
    CMD_CAM_GSP_DOWN_INDEX,
    CMD_CAM_GSY_SET,
    CMD_CAM_PTZ_SET,
    CMD_CAM_SLR_SET,
    CMD_CAM_DZM_SET,

    CMD_CAM_ANGLE_TRACKING,
    CMD_CAM_ANGLE_GIMBAL,
    CMD_CAM_ANGLE_ALT,

    CMD_CAM_ROTATE,
    CMD_CAM_ZOOM,
    CMD_CAM_TILT,

    CMD_SENSOR_TEMP,
    CMD_SENSOR_HUM,
    CMD_EMERGENCYLANDING,

    CMD_STM_PI,
    CMD_STATION_POWER_UPS_SENSOR,
    CMD_STATION_ASMOSPHERE_SENSOR1,
    CMD_STATION_ASMOSPHERE_SENSOR2,

    CMD_STATION_BT,
    CMD_UAV_BT,
    CMD_CODENAME,
    CMD_STREAM_KEY,
    CMD_CAMERA_CODE,

    /* ── SPI camera image-transfer protocol ─────────────────────────────── */
    CMD_SPI_CAM_INFO,    /* master requests image metadata; slave responds with SpiCamInfo  */
    CMD_SPI_CAM_CHUNK,   /* master requests a chunk by index; slave responds with chunk data */

    CMD_COUNT

} CommandType;

#pragma pack(push,1)

typedef struct {
    uint8_t  seq;
    int32_t  lat;
    int32_t  lon;
    int32_t  alt;
    uint16_t yaw;
    uint32_t stationid;
} waypoint;

typedef struct {
    char     serial_number[SERIAL_NUMBER_LENGTH];
    uint32_t time_ms;
} TelemetryHeader;

typedef struct __attribute__((packed)) {
    int32_t lat;
    int32_t lon;
    int32_t alt;
    float   speed;
    uint16_t yaw;
} DronePosition;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* INC_COMMON_DEFS_H_ */
