// Distributed with a free-will license.
// Use it any way you want, profit or free, provided it fits in the licenses of its associated works.
// MMA8452Q
// This code is designed to work with the MMA8452Q_I2CS I2C Mini Module available from ControlEverything.com.
// https://www.controleverything.com/content/Accelorometer?sku=MMA8452Q_I2CS#tabs-0-product_tabset-2

#include "osapi.h"
#include "brzo_i2c.h"
#include "MMA8452Q.h"

#define MMA8452Q_ADDRESS 0x1C
#define MMA8452Q_FREQ    1000

accel currentAccel;

void ICACHE_FLASH_ATTR MMA8452Q_setup(void)
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

	brzo_i2c_end_transaction();
}

void ICACHE_FLASH_ATTR MMA8452Q_poll(void)
{
	// Read 7 bytes of data(0x00)
	brzo_i2c_start_transaction(MMA8452Q_ADDRESS, MMA8452Q_FREQ);

	uint8_t reg[1] = {0x00};
	brzo_i2c_write(reg, sizeof(reg), false);

	uint8_t data[7] = {0};
	brzo_i2c_read(data, sizeof(data), false);

	brzo_i2c_end_transaction();

	if(brzo_i2c_get_error() != 0)
	{
		os_printf("Error : Input/Output error %02X\n", brzo_i2c_get_error());
	}
	else
	{
		// Convert the data to 12-bits
		currentAccel.x = ((data[1] * 256) + data[2]) / 16;
		if(currentAccel.x > 2047)
		{
			currentAccel.x -= 4096;
		}

		currentAccel.y = ((data[3] * 256) + data[4]) / 16;
		if(currentAccel.y > 2047)
		{
			currentAccel.y -= 4096;
		}

		currentAccel.z = ((data[5] * 256) + data[6]) / 16;
		if(currentAccel.z > 2047)
		{
			currentAccel.z -= 4096;
		}
	}
}

accel * ICACHE_FLASH_ATTR getAccel(void)
{
	return &currentAccel;
}
