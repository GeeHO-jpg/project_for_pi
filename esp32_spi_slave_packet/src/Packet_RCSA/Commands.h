/*
 * Commands.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_COMMANDS_H_
#define INC_PACKET_HEADER_COMMANDS_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "common_defs.h" 
/**
 * UDPCommand (1 byte)
 *
 * ตาม Swarm Outdoor Packet Guide:
 *   0  CMD_NONE           No operation (reserved)
 *   1  CMD_HANDSHAKE      Begin handshake sequence
 *   2  CMD_RTCM3          Send RTCM3 GNSS correction data
 *   3  CMD_POWER          Control drone power state (off/on/reset)
 *   4  CMD_SET_ID         Assign or change the drone’s ID
 *   5  CMD_PREARM_CHECK   Perform pre‐arm readiness check
 *   6  CMD_TEST_MOTOR     Run motor test routine
 *   7  CMD_FIRMWARE       Request or send firmware version
 *   8  CMD_VERIFY_PLAN    Verify integrity of uploaded flight plan
 *   9  CMD_FLASH_CHECK    Check flash memory integrity
 *  10  CMD_SET_POINT_XYZ  Set target waypoint coordinates
 *  11  CMD_CAL_COMPASS    Initiate compass calibration
 *  12  CMD_CAL_LEVEL      Initiate level (accelerometer) calibration
 *  13  CMD_SET_LED_RGB    Configure LED colors or pattern mode
 *  14  CMD_FLIGHT_CONTROL Basic flight commands (arm, takeoff, land, etc.)
 *  15  COMMANDS_PREFLIGHT Pre‐flight setup and safety checks
 *  16  CMD_SET_WIFI       Configure Wi-Fi connectivity
 *  17  CMD_UAV_INFO       Telemetry report from drone (1 Hz)
 *  18  CMD_WIFI_INFO      Wi-Fi status report (RSSI, charger status, SSID)
 *  19  CMD_WIFI_UPFILE    Upload plan or parameter file via UDP
 *  20  CMD_SERIAL         Serial data transfer (update or end)
 *  21  CMD_COUNT          จำนวนคำสั่งทั้งหมด
 */
/**
 * UDPCommand (1 byte)
 * ดูรายละเอียดคำสั่ง UDP ที่หน้า 4 ของ Swarm_Packets.pdf
 */

//  typedef enum {

//     CMD_NONE = 0,
//     /* ================= SYSTEM ================= */
//     CMD_HANDSHAKE,
//     CMD_INFO,
//     CMD_REBOOT,

//     /* ================= GNSS ================= */

//     CMD_RTCM,

//     /* ================= FLIGHT ================= */

//     CMD_ARM,
//     CMD_DISARM,
//     CMD_MODE,
//     CMD_MISSION,
//     CMD_TAKEOFF,
//     CMD_LANDING,
//     CMD_RTL,
//     CMD_GOTO,

//     /* ================= FILE ================= */

//     CMD_FILE_REQUEST,
//     CMD_UPLOADFILE,
//     CMD_FILEEOF,

//     /* ================= CAMERA ================= */

//     ROI_LOCK,
//     ROI_CANCEL,
//     ROI_ACK,

//     CMD_CAM_GSP_SET,
//     CMD_CAM_GSP_UP_INDEX,
//     CMD_CAM_GSP_DOWN_INDEX,
//     CMD_CAM_GSY_SET,
//     CMD_CAM_PTZ_SET,
//     CMD_CAM_SLR_SET,
//     CMD_CAM_DZM_SET,

//     CMD_CAM_ANGLE_TRACKING,
//     CMD_CAM_ANGLE_GIMBAL,
//     CMD_CAM_ANGLE_ALT,

//     CMD_CAM_ROTATE,
//     CMD_CAM_ZOOM,
//     CMD_CAM_TILT,
//     /* ================= I2C SENSOR ================= */
//     CMD_SENSOR_TEMP,
//     CMD_SENSOR_HUM,

//     CMD_COUNT


// } Command;


// typedef enum {
//     CMD_NONE = 0,

//     CMD_HANDSHAKE,//ok
//     CMD_INFO,     //ok

//     CMD_RTCM3,//
//     CMD_POWER,//
//     CMD_SET_ID,//
//     CMD_PREARM_CHECK,//Payload None
//     CMD_TEST_MOTOR,//Payload None
//     CMD_FIRMWARE,//
//     CMD_VERIFY_PLAN,//Payload None
//     CMD_FLASH_CHECK,//Payload None
//     CMD_MISSION,
//     CMD_CAL_COMPASS,//Payload None
//     CMD_CAL_LEVEL,//Payload None
//     CMD_SET_LED_RGB,//
//     CMD_ARM,
//     CMD_DISARM,
//     CMD_TAKEOFF,
//     CMD_LAND,
//     CMD_GOTO,
//     CMD_RTL,
//     CMD_SET_GEOFENCE,
//     CMD_RESET_TIME,
//     CMD_SET_RTL_ALT,
//     CMD_SET_COMPASS_OFFSET,
//     CMD_CHECK_FENCE,
//     CMD_CHECK_RTLALT,
//     CMD_SET_WIFI,//
//     CMD_UAV_INFO, // px to wifi to gcs
//     CMD_WIFI_INFO,//   wifi to gcs
//     CMD_WIFI_UPFILE,//udp/rf send open tcp connection

//     CMD_SERIAL_REQUEST_FILEINFO, //REQUEST file info from  flight controller // header
//     CMD_SERIAL_FILEINFO, //file info from  flight controller // header+payload
//     CMD_SERIAL_UPLOAD_FILE,// start upload to  flight controller // upload file to flight controller // header+payload fixsize 1024B+ crc4byte
//     CMD_SERIAL_REQUEST_FILE, //request upload file from wifichip fixsize 1024B  // header+payload <mode 0 = next data mode =1 latest data>
    
//     //CMD_SERIAL_SEND_FILE,//request upload file to flight controller // header+payload fixsize 1024B+ crc4byte
   
   
//     CMD_COUNT
// } Command;



/**
 * TCPCommand (1 byte)
 *
 * ตาม Swarm Outdoor Packet Guide หน้า 5:
 *   0 CMD_NONE               No operation (reserved).
 *   1 CMD_REQUEST_FILE_INFO  Request file metadata (type, size, CRC).
 *   2 CMD_REQUEST_FILE       Request file data (after metadata).
 */
typedef enum {
    TCP_CMD_NONE = 0,
    TCP_CMD_REQUEST_PLAN_INFO,//Request: header only. Reply:file_type(1), size(8), crc32(4).
    TCP_CMD_REQUEST_PLAN_FILE,//Request: header only. Reply: fullplan file bytes.
    TCP_CMD_REQUEST_PARAM_INFO, // Request: header only. Reply:file_type(1), size(8), crc32(4).
    TCP_CMD_REQUEST_PARAM_FILE, // Request: header only. Reply: fullparameter file bytes.
    TCP_CMD_COUNT
} TCPCommand;

typedef enum {
    IN_CMD_BATTERY_VOLTAGE = 0,
    IN_CMD_CHARGER_INFO
} InternalCommand;

/**
 * @brief  ตรวจสอบว่าเป็นคำสั่ง UDP ที่รู้จักหรือไม่
 * @param  cmd  รหัสคำสั่ง (0–CMD_COUNT-1)
 * @return true  ถ้า cmd < CMD_COUNT
 * @return false ถ้าไม่ใช่คำสั่งที่รู้จัก
 */
bool IsValidUDPCommand(uint8_t command_byte);

/**
 * @brief  ตรวจสอบว่าเป็นคำสั่ง TCP ที่รู้จักหรือไม่
 * @param  cmd  รหัสคำสั่ง (0–TCP_CMD_COUNT-1)
 * @return true  ถ้า cmd < TCP_CMD_COUNT
 * @return false ถ้าไม่ใช่คำสั่งที่รู้จัก
 */
bool IsValidTCPCommand(uint8_t command_byte);

#endif /* INC_PACKET_HEADER_COMMANDS_H_ */
