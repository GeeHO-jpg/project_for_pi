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

/* ============================================================
   RTK STATUS
   ============================================================ */

typedef enum {
    RTK_STATUS_NOFIX = 0,
    RTK_STATUS_2DFIX,
    RTK_STATUS_DGPS,
    RTK_STATUS_3DFIX,
    RTK_STATUS_FLOAT,
    RTK_STATUS_FIX
} RCSA_RTK_STATUS;

/* ============================================================
   FILE TRANSFER
   ============================================================ */

typedef enum {
    FILE_START = 0,
    FILE_CHUNK,
    FILE_EOF
} RCSA_FILE;

/* ============================================================
   UAV MODE
   ============================================================ */

typedef enum {
    MODE_STABILIZE = 0,
    MODE_LOITER,
    MODE_GUIDED,
    MODE_AUTO,
    MODE_RTL
} RCSA_MODE;


/* ============================================================
   MISSION COMMAND
   ============================================================ */

typedef enum {
    MISSION_UPLOAD = 0,
    MISSION_START,
    MISSION_STOP,
    MISSION_CLEAR,
    MISSION_STATUS
} RCSA_MISSION;


/* ============================================================
   UAV ARM STATE
   ============================================================ */

typedef enum {
    RCSA_UAV_DISARM = 0,
    RCSA_UAV_ARM
} RCSA_UAV;


/* ============================================================
   UAV STATUS
   ============================================================ */

typedef enum {
    RCSA_UAV_STATUS_OK = 0,
    RCSA_UAV_STATUS_NG
} RCSA_UAV_STATUS;


/* ============================================================
   DOCK STATE
   ============================================================ */

typedef enum {
    DOCK_IDLE = 0,
    DOCK_LANDING,
    DOCK_READY,
    DOCK_LAUNCHING,
    DOCK_ERROR
} DockState;


/* ============================================================
   DOCK DOOR
   ============================================================ */

typedef enum {
    DOOR_CLOSE = 0,
    DOOR_OPEN,
    DOOR_MOVING
} DockDoorState;


/* ============================================================
   CHARGING STATE
   ============================================================ */

typedef enum {
    CHARGE_OFF = 0,
    CHARGE_ON
} ChargingState;


/* ============================================================
   SPI INTERFACE
   ============================================================ */

// typedef enum {
//     SPI_ADDRESS = 56
// } SPIINTERFACE;


typedef enum {
    CHECK_RX_SIZE   =   0x3A,   // Pi ถาม STM: ตอนนี้มีข้อมูลให้อ่านกี่ byte
    READ_PAYLOAD    =   0xFA,   // Pi อ่าน payload จาก STM
    CHECK_TX_FREE   =   0x5A,   // Pi ถาม STM: ตอนนี้ STM รับข้อมูลเพิ่มได้อีกกี่ byte
    WRITE_PAYLOAD   =   0xA5    // Pi ส่ง payload ไป STM
} SPIINTERFACE;

/* hdr(9) + max_payload chunk_data_t(514) + crc(4) + pad(1) = 528, multiple of 4 */
#define SPI_FRAME_SIZE  528u

/* ============================================================
   COMMAND TYPE
   ============================================================ */

/* ============================================================
   COMMAND TYPE
   ============================================================ */

typedef enum {

    CMD_NONE = 0,

    /* ================= SYSTEM ================= */

    CMD_HANDSHAKE,
    CMD_INFO,      //--> STATION SEND TO SERVER
    CMD_REBOOT,

    /* ================= GNSS ================= */
    CMD_RTCM,

    /* ================= FLIGHT ================= */

    CMD_ARM,
    CMD_DISARM,
    CMD_MODE,
    CMD_MISSION,
    CMD_TAKEOFF,
    CMD_LANDING,
    CMD_RTL,
    CMD_GOTO,
    CMD_MOVE,

    /* ================= MISSION PIPELINE ================= */
    CMD_MISSION_REQUEST,
    CMD_MISSION_NEXT_POS,
    CMD_MISSION_OLD_POS,

     /* ================= FILE ================= */
    CMD_UPLOADFILE,
    CMD_FILE_REQUEST_NEXT,
    CMD_FILE_REQUEST_REPEAT,
    CMD_FILE,
    CMD_FILEEOF,

    /* ================= CAMERA ================= */

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

    /* ================= I2C SENSOR ================= */
    CMD_SENSOR_TEMP,
    CMD_SENSOR_HUM,
    CMD_EMERGENCYLANDING,

    CMD_STM_PI,
    CMD_STATION_POWER_UPS_SENSOR,
    CMD_STATION_ENV_SENSOR,
    CMD_STATION_WIND_SENSOR,

    CMD_STATION_BT,           // BluetoothInfo_station
    CMD_UAV_BT,               // BluetoothInfo_UAV

    CMD_GET_IMAGE_INFO,       // Pi5 ขอ metadata ภาพ → ESP32 ตอบ image_info_t
    CMD_GET_CHUNK,            // Pi5 ขอ chunk ภาพ    → ESP32 ตอบ chunk_data_t

    CMD_COUNT
} CommandType;

/* ============================================================
   IMAGE TRANSFER
   ============================================================ */

#define IMAGE_WIDTH       320u
#define IMAGE_HEIGHT      240u
#define IMAGE_CHUNK_SIZE  512u
#define IMAGE_NUM_CHUNKS  150u

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t total_size;
    uint16_t num_chunks;
} image_info_t;  /* 10 bytes */

typedef struct {
    uint16_t chunk_index;
    uint8_t  data[IMAGE_CHUNK_SIZE];
} chunk_data_t;  /* 514 bytes */


/* ============================================================
   DATA STRUCTURES
   ============================================================ */

#pragma pack(push,1)

/* Waypoint */

typedef struct {
    uint8_t  seq;
    int32_t  lat;
    int32_t  lon;
    int32_t  alt;
    uint16_t yaw;
    uint32_t stationid;
} waypoint;

/* Common telemetry header */

typedef struct {
    char     serial_number[SERIAL_NUMBER_LENGTH]; // serial number
    uint32_t time_ms;           // gps_week_ms ฝั่ง server converเป็น utc+7
} TelemetryHeader;


/* UAV status */
typedef struct { 
    uint8_t  uav_type;          // FMU|: 0 drone 1 vtol
    uint8_t  failsafe;          // FMU|:0 normal / 1 failsafe
    uint8_t  gps_status;        // FMU|:NOFIX = 0,2DFIX=1,DGPS=2,3DFIX=3,FLOAT=4,FIX = 5
    uint8_t  satellite;         // FMU|:number of satellites
    uint8_t  armed;             // FMU|:0 disarmed / 1 armed

    uint8_t  mode;              // FMU|:flight mode gride,lan, takeoff ,rtl
    uint8_t  vtol_mode;         // FMU|:0=drone mode, 1 plan mode, 2 transistion mode
    uint8_t  uav_status;        // FMU|:0 = ready ,1 not ready  --> stm32 

    float avoid_front;          // FMU|:sensor avoid_front  m
    float avoid_back;           // FMU|:sensor avoid_back  m
    float avoid_left;           // FMU|:sensor avoid_left  m
    float avoid_right;          // FMU|:sensor avoid_right  m
    float avoid_top;            // FMU|:sensor avoid_top  m
    float ground_distance;      // FMU|:sensor ground_distance  m

    float hdop;                 // FMU|:GPS  hdop

    uint32_t flight_time;       // STM32|:seconds

    uint8_t  battery_pct;       // STM32|:0-100%
    uint16_t voltage;           // FMU|: V
    uint16_t current;           // FMU|:A

    int8_t   signalStrength;    // PI|:RSSI dBm
    uint16_t latency;           // PI|:ms
    uint8_t  simband;           //PI|0 N/A, 1 AIS, 2 TrueMove , 3 DTAC

} DroneStatus;

/* UAV position */

typedef struct __attribute__((packed)) {
    int32_t lat;      // latitude
    int32_t lon;      // longitude
    int32_t alt;      // meters
    float   speed;    // m/s
    uint16_t yaw;     // degrees
} DronePosition;


/*  Station to server */   
typedef struct {
    uint8_t  uav_type;          // Station|: 0 drone 1 vtol
    uint8_t gridPower;          // Station|: 0 NG / 1 OK  
    uint8_t upsstation;         // Station|:UPS active
    uint8_t battery_Backup;     // Station|:UPS battery %

    uint8_t docked;             // Station|:drone docked
    uint8_t dock_state;         // Station|:idle / landing / ready / error
    uint8_t door;               // Station|:0 close / 1 open / 2 moving

    uint8_t  charging;          // Station|:1 on,2 off
    uint16_t chargCurrent;      // Station|:mA
    uint16_t chargVoltage;      // Station|:mV

    int32_t lat;                // Station|:latitude
    int32_t lon;                // Station|:longitude

    uint8_t gps_status;         // Station|:NOFIX = 0,2DFIX=1,DGPS=2,3DFIX=3,FLOAT=4,FIX = 5
    uint8_t satellite;          // Station|:number of satellites

    int8_t temperature;         // Station|:°C      //sensor
    int8_t windEstimate;        //  m/s             //sensor
    uint16_t pressure;            // mmHg           //sensor

    int8_t signalStrength;      // PI|:RSSI dbm   
    uint16_t latency;           // PI|:ms

} UavStation;

/* Drone telemetry packet */
typedef struct {
    TelemetryHeader header;
    DroneStatus     status;
    DronePosition   position;
    char            Dock_home_serial_number[SERIAL_NUMBER_LENGTH]; // serial number
    char            Dock_taget_serial_numberserial_number[SERIAL_NUMBER_LENGTH]; // serial number
} DroneTelemetryPacket;

/* Station telemetry packet */
typedef struct {
    TelemetryHeader header;
    UavStation      station;
} UavStationTelemetryPacket;    //----> sensor

/*********************info station to uav ******************/
typedef struct __attribute__((packed)) {
    TelemetryHeader header;  
    char     uav_serial_number[SERIAL_NUMBER_LENGTH]; // serial number uav in staion
    uint8_t  uav_type;           // 0 drone 1 vtol
    int32_t  lat;                // latitude  in staion
    int32_t  lon;                // longitude in staion
    uint8_t  gps_status;         // NOFIX = 0,2DFIX=1,DGPS=2,3DFIX=3,FLOAT=4,FIX = 5
    uint8_t  satellite;          // number of satellites
    uint8_t  charging;          // 0 off / 1 on
    uint16_t chargCurrent;      // A
    uint16_t chargVoltage;      // V
} stationBluetoothInfo;

/*********************info uav to station ******************/
typedef struct __attribute__((packed)) {
    TelemetryHeader     header;         
    uint8_t             uav_type;       // 0 drone 1 vtol
    uint8_t             armed;          // 0 disarmed / 1 armed
    uint8_t             uav_status;     // 0 = ready ,1 not ready
    DronePosition       position;       // Position
    uint8_t             gps_status;  
    char                Dock_home_serial_number[SERIAL_NUMBER_LENGTH]; // serial number
    char                Dock_taget_serial_numberserial_number[SERIAL_NUMBER_LENGTH]; // serial number
    uint8_t             charging;           // 0 off / 1 on
    uint8_t             battery_pct;        //0-100%
    uint16_t            voltage;            //V
    uint16_t            current;            //A
    uint16_t            chargCurrent;       // A
    uint16_t            chargVoltage;       // V
} UavBluetoothInfo;

/*================= STATION IDENTITY [เฉพาะ PI] ================= */
typedef struct __attribute__((packed)) {
   char Dock_home_serial_number[SERIAL_NUMBER_LENGTH]; // serial number
} DockIdentity;

/* ================= BLUETOOTH LINK ================= */
typedef enum {

    /* station -> drone */
    CMD_STATION_INFO            = 100,      // stationBluetoothInfo
    CMD_BT_LANDING_PERMISSION,
    CMD_BT_CHARGE_STATUS,

    /* drone -> station */
    CMD_UAV_INFO,          // UavBluetoothInfo
    CMD_BT_UAV_APPROACH,
    CMD_BT_UAV_LANDED,

} BluetoothCommand;



/**
 * condition 
 * 
 * ***UAV → Station Condition
 * 
 * - if armed == 1            
    --> reject_charging

   - if Dock_taget_serial_number != station_serial
    --> ignore_packet

    distance = calc_distance(UAV.position , station.position)
   - if distance > 100m
    -->ignore

  **** Station → UAV Condition
   - if gps_status < Nofix
     -->reject_landing ->>   ถามโอ๋

   - if charging == 1 AND UAV still flying -< check arm
     -->error

   - if uav_serial_number != my_serial
    -->ignore_packet (fuction[])

  *****Charging Safety Logic****
   - if UAV landed AND armed == 0
      charging = 1
    else
      charging = 0

   - if chargeVoltage < 10V
     charging_fault not  charging_error

   - if chargeCurrent > MAX_CHARGE_CURRENT
    stop_charge   
 * 
 * 
*/

/** 
 * 
1) Station -> UAV   : ส่ง stationBluetoothInfo
2) UAV รับ + ตรวจเงื่อนไข
3) UAV -> Station   : ส่ง UavBluetoothInfo
4) Station รับ + ตรวจเงื่อนไข + ตัดสินใจ charging / reject / allow
5) Station -> UAV   : ส่ง stationBluetoothInfo รอบถัดไปพร้อม state ล่าสุด
6) วนซ้ำเป็น heartbeat
* 
* 
*/

// func (c *Client) SendTelemetryLoop(ctx context.Context, droneID int) {

// 	ticker := time.NewTicker(time.Second)
// 	defer ticker.Stop()

// 	for {
// 		select {

// 		case <-ctx.Done():
// 			return

// 		case <-ticker.C:

// 			t := TelemetryMessage{
// 				DroneCode:      fmt.Sprintf("DRN-%03d", droneID),
// 				DroneName:      "Drone Alpha",
// 				Model:          "PX4-X1",
// 				Status:         "flying",

// 				BatteryPercent: 87,
// 				SpeedKmh:       32,
// 				AltitudeM:      120,
// 				HeadingDeg:     90,

// 				Lat: 13.7563,
// 				Lng: 100.5024,
// 			}

// 			if err := c.SendTelemetry(ctx, t); err != nil {
// 				c.log.Error("Telemetry send failed",
// 					"drone_code", t.DroneCode,
// 					"err", err)
// 			}
// 		}
// 	}
// }
#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* INC_COMMON_DEFS_H_ */
