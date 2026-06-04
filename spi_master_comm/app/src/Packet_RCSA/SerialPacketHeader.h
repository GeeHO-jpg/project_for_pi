/*
 * SerialPacketHeader.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_SERIALPACKETHEADER_H_
#define INC_PACKET_HEADER_SERIALPACKETHEADER_H_

#include <stdint.h>

#define SERIALPACKETHEADER_SIZE     3

typedef struct {
    uint8_t cmd;            // 1-byte command
    uint16_t payload_size;  // 2-byte payload size
}SerialPacketHeader_t;

#endif /* INC_PACKET_HEADER_SERIALPACKETHEADER_H_ */
