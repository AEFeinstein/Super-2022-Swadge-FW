// Distributed with a free-will license.
// Use it any way you want, profit or free, provided it fits in the licenses of its associated works.
// MMA8452Q
// This code is designed to work with the MMA8452Q_I2CS I2C Mini Module available from ControlEverything.com.
// https://www.controleverything.com/content/Accelorometer?sku=MMA8452Q_I2CS#tabs-0-product_tabset-2

#include <osapi.h>

#include "brzo_i2c.h"
#include "user_main.h"
#include "MMA8452Q.h"

#define MMA8452Q_ADDRESS 0x1C
#define MMA8452Q_FREQ    400

bool ICACHE_FLASH_ATTR MMA8452Q_setup(void)
{
    brzo_i2c_start_transaction(MMA8452Q_ADDRESS, MMA8452Q_FREQ);

    // Select mode register(0x2A)
    // Standby mode(0x00)
    uint8_t config[2] = {0};
    config[0] = 0x2A;
    config[1] = 0x00;
    brzo_i2c_write(config, sizeof(config), false);

    // Select mode register(0x2A)
    // Active mode(0x01)
    config[0] = 0x2A;
    config[1] = 0x01;
    brzo_i2c_write(config, sizeof(config), false);

    // Select configuration register(0x0E)
    // Set range to +/- 2g(0x00)
    config[0] = 0x0E;
    config[1] = 0x00;
    brzo_i2c_write(config, sizeof(config), false);

    return (0 == brzo_i2c_end_transaction());
}

void ICACHE_FLASH_ATTR MMA8452Q_poll(accel_t* currentAccel)
{
    // Read 7 bytes of data(0x00)
    brzo_i2c_start_transaction(MMA8452Q_ADDRESS, MMA8452Q_FREQ);

    uint8_t reg[1] = {0x00};
    brzo_i2c_write(reg, sizeof(reg), false);

    uint8_t data[7] = {0};
    brzo_i2c_read(data, sizeof(data), false);

    uint8_t completion_code = brzo_i2c_end_transaction();

    if(completion_code != 0)
    {
        os_printf("Error : Input/Output error %02X\n", completion_code);
    }
    else
    {
        // Convert the data to 12-bits
        currentAccel->x = (data[1] << 4) | ((data[2] >> 4) & 0x0F);
        if(currentAccel->x & 0x0800)
        {
            currentAccel->x |= 0xF000;
        }

        currentAccel->y = (data[3] << 4) | ((data[4] >> 4) & 0x0F);
        if(currentAccel->y & 0x0800)
        {
            currentAccel->y |= 0xF000;
        }

        currentAccel->z = (data[5] << 4) | ((data[6] >> 4) & 0x0F);
        if(currentAccel->z & 0x0800)
        {
            currentAccel->z |= 0xF000;
        }

        // QMA6981 is only 10 bits, so go from 12 to 10 here for consistency
        currentAccel->x /= 4;
        currentAccel->y /= 4;
        currentAccel->z /= 4;
    }
}
