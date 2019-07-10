/*
 * QMA6981.c
 *
 *  Created on: Jul 9, 2019
 *      Author: adam
 */

#include <osapi.h>
#include "brzo_i2c.h"
#include "QMA6981.h"

#define QMA6981_ADDR 0x12
#define QMA6981_FREQ  400

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool QMA6981_setup(void)
{
    brzo_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);
    return (0 == brzo_i2c_end_transaction()) ? true : false;
}

/**
 * @brief
 *
 * @param currentAccel
 */
void QMA6981_poll(accel_t* currentAccel)
{
    brzo_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);
    brzo_i2c_end_transaction();

    currentAccel->x = 0;
    currentAccel->y = 0;
    currentAccel->z = 0;
}