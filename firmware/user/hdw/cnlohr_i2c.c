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

#define CNLOHR_I2C_SDA_MUX PERIPHS_IO_MUX_GPIO2_U
#define CNLOHR_I2C_SCL_MUX PERIPHS_IO_MUX_GPIO0_U
#define CNLOHR_I2C_SDA_FUNC FUNC_GPIO2
#define CNLOHR_I2C_SCL_FUNC FUNC_GPIO0

int cnl_need_new_stop;
uint8_t cnl_slave_address;
int cnl_err;
bool cnl_highSpeed;

void ICACHE_FLASH_ATTR ConfigI2C(void)
{
    GPIO_DIS_OUTPUT(REMAP(I2CSDA));
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 1);
}

void SendStart(bool highSpeed)
{
    my_i2c_delay(highSpeed);
    my_i2c_delay(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSDA), 0);
    my_i2c_delay(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 0);
    my_i2c_delay(highSpeed);
}

void SendStop(bool highSpeed)
{
    my_i2c_delay(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSDA), 0);  //May or may not be done.
    my_i2c_delay(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 0);  //Should already be done.
    my_i2c_delay(highSpeed);
    GPIO_OUTPUT_SET(REMAP(I2CSCL), 1);
    my_i2c_delay(highSpeed);
    GPIO_DIS_OUTPUT(REMAP(I2CSDA));
    my_i2c_delay(highSpeed);
}

//Return nonzero on failure.
unsigned char SendByte( unsigned char data, bool highSpeed )
{
    unsigned char i;
    PIN_OUT_SET = (1 << I2CSDA);
    PIN_DIR_OUTPUT = 1 << I2CSDA;
    my_i2c_delay(highSpeed);
    PIN_DIR_OUTPUT = 1 << I2CSCL;
    for( i = 0; i < 8; i++ )
    {
        my_i2c_delay(highSpeed);
        if( data & 0x80 )
        {
            PIN_OUT_SET = (1 << I2CSDA);
        }
        else
        {
            PIN_OUT_CLEAR = (1 << I2CSDA);
        }
        data <<= 1;
        my_i2c_delay(highSpeed);
        PIN_OUT_SET = (1 << I2CSCL);
        my_i2c_delay(highSpeed);
        my_i2c_delay(highSpeed);
        PIN_OUT_CLEAR = (1 << I2CSCL);
    }

    //Immediately after sending last bit, open up DDDR for control.
    //WARNING: this does mean on "read"s from the accelerometer, there is a VERY brief (should be less than 150ns) contradiction.
    //This should have no ill effects.
    PIN_DIR_INPUT = (1 << I2CSDA);
    my_i2c_delay(highSpeed);
    PIN_OUT_SET = (1 << I2CSCL);
    my_i2c_delay(highSpeed);
    my_i2c_delay(highSpeed);
    i = (PIN_IN & (1 << I2CSDA)) ? 1 : 0; //Read in input.  See if client is there.
    PIN_OUT_CLEAR = (1 << I2CSCL);
    my_i2c_delay(highSpeed);
    return (i) ? 1 : 0;
}

unsigned char GetByte( uint8_t send_nak, bool highSpeed)
{
    unsigned char i;
    unsigned char ret = 0;

    PIN_DIR_INPUT = 1 << I2CSDA;

    for( i = 0; i < 8; i++ )
    {
        my_i2c_delay(highSpeed);
        PIN_OUT_SET = (1 << I2CSCL);
        my_i2c_delay(highSpeed);
        my_i2c_delay(highSpeed);
        ret <<= 1;
        if( PIN_IN & (1 << I2CSDA) )
        {
            ret |= 1;
        }
        PIN_OUT_CLEAR = (1 << I2CSCL);
        my_i2c_delay(highSpeed);
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
    my_i2c_delay(highSpeed);
    PIN_OUT_SET = (1 << I2CSCL);
    my_i2c_delay(highSpeed);
    my_i2c_delay(highSpeed);
    my_i2c_delay(highSpeed);
    PIN_OUT_CLEAR = (1 << I2CSCL);
    my_i2c_delay(highSpeed);

    return ret;
}

void my_i2c_delay(bool highSpeed)
{
    asm volatile("nop\nnop\n"); // Less than two nops causes a sad face :(
    asm volatile("nop\nnop\n"); // Four nops work, but have eventual screen glitches
    asm volatile("nop\nnop\n");
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
    }

    return;
}

void cnlohr_i2c_setup( uint32_t clock_stretch_time_out_usec __attribute__((unused)))
{
    PIN_FUNC_SELECT(CNLOHR_I2C_SDA_MUX, CNLOHR_I2C_SDA_FUNC);
    PIN_FUNC_SELECT(CNLOHR_I2C_SCL_MUX, CNLOHR_I2C_SCL_FUNC);

    //~60k resistor. On a small PCB, with 3 I2C clients, it takes ~ 1uS to get to 2v
    PIN_PULLUP_EN(CNLOHR_I2C_SDA_MUX);
    PIN_PULLUP_EN(CNLOHR_I2C_SCL_MUX);

    ConfigI2C();
}


void cnlohr_i2c_write(const uint8_t* data, uint32_t no_of_bytes, bool repeated_start)
{
    unsigned i;
    if( cnl_need_new_stop && !repeated_start )
    {
        SendStop(cnl_highSpeed);
        my_i2c_delay(cnl_highSpeed);
    }
    SendStart(cnl_highSpeed);
    if( SendByte( cnl_slave_address << 1, cnl_highSpeed ) )
    {
        cnl_err = 1;
    }
    for( i = 0; i < no_of_bytes; i++ )
    {
        if( SendByte( data[i], cnl_highSpeed ) )
        {
            cnl_err = 1;
        }
    }
    cnl_need_new_stop = 1;
}

void cnlohr_i2c_start_transaction(uint8_t slave_address, uint16_t SCL_frequency_KHz)
{
    cnl_highSpeed = (800 == SCL_frequency_KHz);
    cnl_err = 0;
    // SendStart();
    // SendByte( slave_address << 1 );
    cnl_slave_address = slave_address;
}


void cnlohr_i2c_read(uint8_t* data, uint32_t nr_of_bytes, bool repeated_start)
{
    if( cnl_need_new_stop && !repeated_start )
    {
        SendStop(cnl_highSpeed);
        my_i2c_delay(cnl_highSpeed);
    }

    SendStart(cnl_highSpeed);
    if( SendByte( 1 | (cnl_slave_address << 1), cnl_highSpeed ) )
    {
        cnl_err = 1;
    }

    // if( repeated_start ) SendStart();
    unsigned i;
    for( i = 0; i < nr_of_bytes - 1; i++ )
    {
        data[i] = GetByte( 0, cnl_highSpeed );
    }
    data[i] = GetByte( 1, cnl_highSpeed );
    SendStop(cnl_highSpeed);
}

uint8_t cnlohr_i2c_end_transaction(void)
{
    SendStop(cnl_highSpeed);
    cnl_need_new_stop = 0;
    return cnl_err;
}