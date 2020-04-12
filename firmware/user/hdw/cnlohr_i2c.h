//Copyright 2014 <>< Charles Lohr
//This file may be used for any purposes (commercial or private) just please leave this copyright notice in there somewhere.  Originally based off of static_i2c.h for AVRs (also by Charles Lohr).

#ifndef _I2C_H
#define _I2C_H

#include <ets_sys.h>
#include <gpio.h>

#define I2CSDA 2
#define I2CSCL 0

//#define my_i2c_delay ets_delay_us( 1 );
void my_i2c_delay(bool);

//Assumes I2CGet was already called.
void ConfigI2C(void);

void SendStart(bool);
void SendStop(bool);
//Return nonzero on failure.
unsigned char SendByte( unsigned char data, bool );
unsigned char GetByte( uint8_t send_nak, bool );

void cnlohr_i2c_write(const uint8_t* data, uint32_t no_of_bytes, bool repeated_start);
void cnlohr_i2c_start_transaction(uint8_t slave_address, uint16_t SCL_frequency_KHz);
void cnlohr_i2c_read(uint8_t* data, uint32_t nr_of_bytes, bool repeated_start);
uint8_t cnlohr_i2c_end_transaction(void);

//Do nothing, will be handled on first transaction.
void cnlohr_i2c_setup(uint32_t clock_stretch_time_out_usec);

#endif
