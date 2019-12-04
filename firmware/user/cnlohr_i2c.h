//Copyright 2014 <>< Charles Lohr
//This file may be used for any purposes (commercial or private) just please leave this copyright notice in there somewhere.  Originally based off of static_i2c.h for AVRs (also by Charles Lohr).

#ifndef _I2C_H
#define _I2C_H

#include <ets_sys.h>
#include <gpio.h>

#define I2CSDA 2
#define I2CSCL 0

#define I2CDELAY my_i2c_delay();
//#define I2CDELAY ets_delay_us( 1 );
void my_i2c_delay(void);

//Assumes I2CGet was already called.
void ConfigI2C(void);

void SendStart(void);
void SendStop(void);
//Return nonzero on failure.
unsigned char SendByte( unsigned char data );
unsigned char GetByte( uint8_t send_nak );

#endif
