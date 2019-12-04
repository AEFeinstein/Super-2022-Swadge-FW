#include "cnlohr_i2c.h"

//#define REMAP(x) GPIO_ID_PIN(x)
#define REMAP(x) x

#define PIN_OUT       ( *((uint32_t*)0x60000300) )
#define PIN_OUT_SET   ( *((uint32_t*)0x60000304) )
#define PIN_OUT_CLEAR ( *((uint32_t*)0x60000308) )
#define PIN_DIR       ( *((uint32_t*)0x6000030C) )
#define PIN_DIR_OUTPUT ( *((uint32_t*)0x60000310) )
#define PIN_DIR_INPUT ( *((uint32_t*)0x60000314) )
#define PIN_IN        ( *((volatile uint32_t*)0x60000318) )


void ICACHE_FLASH_ATTR ConfigI2C()
{
	GPIO_DIS_OUTPUT(REMAP(I2CSDA));
	GPIO_OUTPUT_SET(REMAP(I2CSCL),1);
}

void SendStart()
{
	I2CDELAY
	I2CDELAY
	GPIO_OUTPUT_SET(REMAP(I2CSDA), 0);
	I2CDELAY
	GPIO_OUTPUT_SET(REMAP(I2CSCL), 0);
	I2CDELAY
}

void SendStop()
{
	I2CDELAY
	GPIO_OUTPUT_SET(REMAP(I2CSDA), 0);  //May or may not be done.
	I2CDELAY
	GPIO_OUTPUT_SET(REMAP(I2CSCL), 0);  //Should already be done.
	I2CDELAY
	GPIO_OUTPUT_SET(REMAP(I2CSCL), 1);
	I2CDELAY
	GPIO_DIS_OUTPUT(REMAP(I2CSDA));
	I2CDELAY
}

//Return nonzero on failure.
unsigned char SendByte( unsigned char data )
{
	unsigned char i;
	PIN_OUT_SET = (1<<I2CSDA);
	PIN_DIR_OUTPUT = 1<<I2CSDA;
	I2CDELAY
	PIN_DIR_OUTPUT = 1<<I2CSCL;
	for( i = 0; i < 8; i++ )
	{
		I2CDELAY
		if( data & 0x80 )
			PIN_OUT_SET = (1<<I2CSDA);
		else
			PIN_OUT_CLEAR = (1<<I2CSDA);
		data<<=1;
		I2CDELAY
		PIN_OUT_SET = (1<<I2CSCL);
		I2CDELAY
		I2CDELAY
		PIN_OUT_CLEAR = (1<<I2CSCL);
	}

	//Immediately after sending last bit, open up DDDR for control.
	PIN_DIR_INPUT = (1<<I2CSDA);
	I2CDELAY
	PIN_OUT_SET = (1<<I2CSCL);
	I2CDELAY
	I2CDELAY
	i = (PIN_IN & (1<<I2CSDA))?1:0; //Read in input.  See if client is there.
	PIN_OUT_CLEAR = (1<<I2CSCL);
	I2CDELAY
	return (i)?1:0;
}

unsigned char GetByte( uint8_t send_nak )
{
	unsigned char i;
	unsigned char ret = 0;

	PIN_DIR_INPUT = 1<<I2CSDA;

	for( i = 0; i < 8; i++ )
	{
		I2CDELAY
		PIN_OUT_SET = (1<<I2CSCL);
		I2CDELAY
		I2CDELAY
		ret<<=1;
		if( PIN_IN & (1<<I2CSDA) )
			ret |= 1;
		PIN_OUT_CLEAR = (1<<I2CSCL);
		I2CDELAY
	}

	PIN_DIR_OUTPUT = 1<<I2CSDA;

	//Send ack.
	if( send_nak )
	{
		PIN_OUT_SET = (1<<I2CSDA);
	}
	else
	{
		PIN_OUT_CLEAR = (1<<I2CSDA);
	}
	I2CDELAY
	PIN_OUT_SET = (1<<I2CSCL);
	I2CDELAY
	I2CDELAY
	I2CDELAY
	PIN_OUT_CLEAR = (1<<I2CSCL);
	I2CDELAY

	return ret;
}

void my_i2c_delay()
{
	asm volatile("nop\nnop\n");	 //Less than 2 causes a sad face :( 
	return;
}


