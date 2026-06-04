/*
 * IDNumber.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#ifndef INC_PACKET_HEADER_IDNUMBER_H_
#define INC_PACKET_HEADER_IDNUMBER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define IDNUMBER_BROADCAST  0

typedef struct {
    uint16_t id;
} IDNumber;

extern IDNumber* drone_id;

bool IsInitializeIDNumber();
void InitializeIDNumber();

bool IsValidIDNumber(uint16_t id);
void SetIDNumber(uint16_t id);

#endif /* INC_PACKET_HEADER_IDNUMBER_H_ */
