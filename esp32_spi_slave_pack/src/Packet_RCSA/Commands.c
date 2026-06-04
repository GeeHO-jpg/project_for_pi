/*
 * Commands.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#include "Commands.h"

bool IsValidUDPCommand(uint8_t command_byte)
{
    return command_byte <  CMD_COUNT;
}
bool IsValidTCPCommand(uint8_t command_byte) {
    return (command_byte < TCP_CMD_COUNT);
}




