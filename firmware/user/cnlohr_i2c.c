#include "cnlohr_i2c.h"

//#define REMAP(x) GPIO_ID_PIN(x)
#define REMAP(x) x

// #define PIN_OUT       ( *((uint32_t*)0x60000300) )
#define PIN_OUT_SET   ( *((uint32_t*)0x60000304) )
#define PIN_OUT_CLEAR ( *((uint32_t*)0x60000308) )
// #define PIN_DIR       ( *((uint32_t*)0x6000030C) )
#define PIN_DIR_OUTPUT ( *((uint32_t*)0x60000310) )
#define PIN_DIR_INPUT ( *((uint32_t*)0x60000314) )
#define PIN_IN        ( *((volatile uint32_t*)0x60000318) )


void ICACHE_FLASH_ATTR ConfigI2C(void)
{
    GPIO_DIS_OUTPUT(REMAP(I2CSDA));
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 1);
}

void SendStart(bool highSpeed)
{
    I2CDELAY(highSpeed);
    I2CDELAY(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSDA), 0);
    I2CDELAY(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 0);
    I2CDELAY(highSpeed);
}

void SendStop(bool highSpeed)
{
    I2CDELAY(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSDA), 0);  //May or may not be done.
    I2CDELAY(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 0);  //Should already be done.
    I2CDELAY(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 1);
    I2CDELAY(highSpeed);
    GPIO_DIS_OUTPUT(REMAP(I2CSDA));
    I2CDELAY(highSpeed);
}

//Return nonzero on failure.
unsigned char SendByte( unsigned char data, bool highSpeed )
{
    unsigned char i;
    PIN_OUT_SET = (1 << I2CSDA);
    PIN_DIR_OUTPUT = 1 << I2CSDA;
    I2CDELAY(highSpeed);
    PIN_DIR_OUTPUT = 1 << I2CSCL;
    for( i = 0; i < 8; i++ )
    {
        I2CDELAY(highSpeed);
        if( data & 0x80 )
        {
            PIN_OUT_SET = (1 << I2CSDA);
        }
        else
        {
            PIN_OUT_CLEAR = (1 << I2CSDA);
        }
        data <<= 1;
        I2CDELAY(highSpeed);
        PIN_OUT_SET = (1 << I2CSCL);
        I2CDELAY(highSpeed);
        I2CDELAY(highSpeed);
        PIN_OUT_CLEAR = (1 << I2CSCL);
    }

    //Immediately after sending last bit, open up DDDR for control.
    //WARNING: this does mean on "read"s from the accelerometer, there is a VERY brief (should be less than 150ns) contradiction.
    //This should have no ill effects.
    PIN_DIR_INPUT = (1 << I2CSDA);
    I2CDELAY(highSpeed);
    PIN_OUT_SET = (1 << I2CSCL);
    I2CDELAY(highSpeed);
    I2CDELAY(highSpeed);
    i = (PIN_IN & (1 << I2CSDA)) ? 1 : 0; //Read in input.  See if client is there.
    PIN_OUT_CLEAR = (1 << I2CSCL);
    I2CDELAY(highSpeed);
    return (i) ? 1 : 0;
}

unsigned char GetByte( uint8_t send_nak, bool highSpeed)
{
    unsigned char i;
    unsigned char ret = 0;

    PIN_DIR_INPUT = 1 << I2CSDA;

    for( i = 0; i < 8; i++ )
    {
        I2CDELAY(highSpeed);
        PIN_OUT_SET = (1 << I2CSCL);
        I2CDELAY(highSpeed);
        I2CDELAY(highSpeed);
        ret <<= 1;
        if( PIN_IN & (1 << I2CSDA) )
        {
            ret |= 1;
        }
        PIN_OUT_CLEAR = (1 << I2CSCL);
        I2CDELAY(highSpeed);
    }

    PIN_DIR_OUTPUT = 1 << I2CSDA;

    //Send ack.
    if( send_nak )
    {
        PIN_OUT_SET = (1 << I2CSDA);
    }
    else
    {
        PIN_OUT_CLEAR = (1 << I2CSDA);
    }
    I2CDELAY(highSpeed);
    PIN_OUT_SET = (1 << I2CSCL);
    I2CDELAY(highSpeed);
    I2CDELAY(highSpeed);
    I2CDELAY(highSpeed);
    PIN_OUT_CLEAR = (1 << I2CSCL);
    I2CDELAY(highSpeed);

    return ret;
}

void my_i2c_delay(bool highSpeed)
{
    asm volatile("nop\nnop\n"); // Less than 2 causes a sad face :(
    asm volatile("nop\nnop\n"); // More than 2 makes Snortmelon crash.
    if(!highSpeed)
    {
        asm volatile("nop\nnop\n");  // Wait a lot longer
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
        asm volatile("nop\nnop\n");
    }

    return;
}


