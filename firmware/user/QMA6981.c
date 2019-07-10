/*
 * QMA6981.c
 *
 *  Created on: Jul 9, 2019
 *      Author: adam
 */

// Datasheet at
// https://datasheet.lcsc.com/szlcsc/QST-QMA6981_C310611.pdf
// Some values taken from
// https://github.com/yangzhiqiang723/rainbow-RB59M325ALB/blob/02ea6fc2a7f9744273b850cff751ffd2fcf1820b/src/qmaX981.c
// https://github.com/yangzhiqiang723/rainbow-RB59M325ALB/blob/02ea6fc2a7f9744273b850cff751ffd2fcf1820b/inc/qmaX981.h

#include <osapi.h>
#include "brzo_i2c.h"
#include "QMA6981.h"

#define QMA6981_ADDR 0x12
#define QMA6981_FREQ  400

/*ODR SET @lower ODR*/
#define QMA6981_ODR_250HZ           0x0d
#define QMA6981_ODR_125HZ           0x0c
#define QMA6981_ODR_62HZ            0x0b
#define QMA6981_ODR_31HZ            0x0a
#define QMA6981_ODR_16HZ            0x09


/* Accelerometer Sensor Full Scale */
#define QMAX981_RANGE_2G            0x01
#define QMAX981_RANGE_4G            0x02
#define QMAX981_RANGE_8G            0x04
#define QMAX981_RANGE_16G           0x08
#define QMAX981_RANGE_32G           0x0f

typedef enum
{
    CHIP_ID = 0x00,
    DATA = 0x01,
    STEP_CNT = 0x07,
    INT_STATUS = 0x0A,
    FIFO_STATUS = 0x0E,
    FULL_SCALE = 0x0F,
    BW = 0x10,
    POWER_MODE = 0x11,
    STEP_CONF = 0x13,
    INT_EN = 0x16,
    INT_SRC = 0x18,
    INT_MAP = 0x19,
    INT_PIN_CONF = 0x20,
    INT_LATCH = 0x21,
    LowG_HighG = 0x22,
    OS_CUST = 0x27,
    TAP = 0x2A,
    _4D_6D = 0x2C,
    FIFO_WM = 0x31,
    SelfTest = 0x32,
    NVM_CFG = 0x33,
    SOFT_RESET = 0x36,
    IMAGE = 0x37,
    FIFO_CONF = 0x3E,
} QMA6981_reg_addr;

void qmaX981_writereg(uint8_t addr, uint8_t data);

/**
 * @brief
 *
 * @return true
 * @return false
 */
bool QMA6981_setup(void)
{
    // Read 1 byte of data(0x00)
    brzo_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);

    uint8_t reg[1] = {CHIP_ID};
    brzo_i2c_write(reg, sizeof(reg), false);

    uint8_t data[1] = {0};
    brzo_i2c_read(data, sizeof(data), false);

    uint8_t err = brzo_i2c_end_transaction();

    if(0 == err)
    {
        os_printf("QMA6981 chip ID %d\n", data[0]);

        brzo_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);

        qmaX981_writereg(POWER_MODE, 0x80);
        qmaX981_writereg(SOFT_RESET, 0xb6);
        qmaX981_writereg(0xff, 5);
        qmaX981_writereg(SOFT_RESET, 0x00);
        qmaX981_writereg(POWER_MODE, 0x80);
        os_delay_us(5);
        qmaX981_writereg(FULL_SCALE, QMAX981_RANGE_4G);
        qmaX981_writereg(BW, QMA6981_ODR_125HZ);

        qmaX981_writereg(BW, 0x05);
        qmaX981_writereg(POWER_MODE, 0x80); // 0x85 {0x2a, 0x80},
        qmaX981_writereg(0x2b, 0x03);   //0x14  125*7
        qmaX981_writereg(INT_EN, 0x20);
        qmaX981_writereg(INT_MAP, 0x20);
        qmaX981_writereg(INT_PIN_CONF, 0x00);

        return (0 == brzo_i2c_end_transaction()) ? true : false;
    }
    else
    {
        os_printf("Couldn't read QMA6981 chip ID, err: %d\n", err);
    }
}

/**
 * @brief
 *
 * @param currentAccel
 */
void QMA6981_poll(accel_t* currentAccel)
{
    // Read 7 bytes of data(0x00)
    brzo_i2c_start_transaction(QMA6981_ADDR, QMA6981_FREQ);

    uint8_t reg[1] = {DATA};
    brzo_i2c_write(reg, sizeof(reg), false);

    uint8_t data[6] = {0};
    brzo_i2c_read(data, sizeof(data), false);

    uint8_t err = brzo_i2c_end_transaction();

    if(err != 0)
    {
        os_printf("Error : Input/Output error %02X\n", err);
    }
    else
    {
        // Convert the data to 12-bits
        currentAccel->x = ((data[0] >> 1 ) | (data[1]) << 7);
        currentAccel->y = ((data[2] >> 1 ) | (data[3]) << 7);
        currentAccel->z = ((data[4] >> 1 ) | (data[5]) << 7);
    }
}

/**
 * @brief TODO
 *
 * @param addr
 * @param data
 */
void qmaX981_writereg(uint8_t addr, uint8_t data)
{
    uint8_t writeCmd[2] = {addr, data};
    brzo_i2c_write(writeCmd, sizeof(writeCmd), false);
}