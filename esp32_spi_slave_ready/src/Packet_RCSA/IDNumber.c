/*
 * IDNumber.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Lenovo
 */

#include "IDNumber.h"


IDNumber* drone_id = NULL;

bool IsInitializeIDNumber()
{
    return drone_id != NULL;
}

void InitializeIDNumber()
{
    if (IsInitializeIDNumber())
        return;
    
    drone_id = (IDNumber*)malloc(sizeof(IDNumber));
    if (drone_id == NULL)
        return;

    drone_id->id = 0;
}

bool IsValidIDNumber(uint16_t id)
{
    return (id == IDNUMBER_BROADCAST || (IsInitializeIDNumber() && id == drone_id->id));
}

void SetIDNumber(uint16_t id)
{
    if (!IsInitializeIDNumber())
        return;
    
    if (drone_id->id != id)
        drone_id->id = id;
}
