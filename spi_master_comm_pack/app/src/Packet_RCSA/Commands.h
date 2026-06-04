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

typedef enum {
    TCP_CMD_NONE = 0,
    TCP_CMD_REQUEST_PLAN_INFO,
    TCP_CMD_REQUEST_PLAN_FILE,
    TCP_CMD_REQUEST_PARAM_INFO,
    TCP_CMD_REQUEST_PARAM_FILE,
    TCP_CMD_COUNT
} TCPCommand;

bool IsValidUDPCommand(uint8_t command_byte);
bool IsValidTCPCommand(uint8_t command_byte);

#endif /* INC_PACKET_HEADER_COMMANDS_H_ */
