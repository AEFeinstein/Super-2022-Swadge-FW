# MAGStock 2019 Swadges base off of ESP8266 ColorChord

## This readme is from colorchord.

## WARNING THIS IS UNDER CONSTRUCTION

(based off of ESP8266 I2S WS2812 Driver)

This project is based off of the I2S interface for the mp3 player found here:
https://github.com/espressif/esp8266_mp3_decoder/

If you want more information about the build environment, etc.  You should
check out the regular WS2812 driver, found here: https://github.com/cnlohr/esp82xx and https://github.com/cnlohr/esp8266ws2812i2s

WARNING: This subproject is very jankey!  It's about stable, but I don't think it's quite there yet.

## Hardware Connection

Unfortunately the I2S Out (WS2812 in) pin is the same as RX1 (pin 25), which means if you are programming via the UART, it'll need to be unplugged any time you're testing.  The positive side of this is that it is a pin that is exposed on most ESP8266 breakout boards.

The audio data is taken from TOUT, but must be kept between 0 and 1V.

## Notes

./makeconf.inc has a few variables that Make uses for building and flashing the firmware.
Most notably the location of the toolchain for the esp8266/85 on your system.
You should edit them according to your preferences or better add `export ESP_ROOT='/path/to/the/esp-open-sdk'` to your bashrc.

If you have problems with burning the firmware or transfering page data over network (`make netburn` or `make netweb`), you should try uncommenting

    OPTS += -DVERIFY_FLASH_WRITE

in `makeconf.inc`. This way the esp checks if the flash is written correctly.
Especially with some ESP-01 modules there has been a problem with the flash
not being written correctly.

## UDP Commands

These commands can be sent to port 7878, defined in user.cfg. Custom commands from custom_commands.c all start with 'C'. All others from commonservices.c do not. The non-custom commands reference can be found at https://github.com/cnlohr/esp82xx#commands

| Command | Name | Description |
| -------------- | ---- | ----------- |
| CB | Bins | Given an integer, return the bins vals in a string over UDP. 0 == embeddedBins32, 1 == fizzed_bins, 3 == folded_bins |
| CL | LEDs | Return the LED values in a string over UDP |
| CM | Oscilloscope | Return the sounddata values in a string over UDP |
| CN | Notes | Return the note peak frequency, peak amplitudes, and jumps in a string over UDP |
| CSD | Config Default | Set all configurables to default values | 
| CSR | Config Read | Read all configurables from struct SaveLoad |
| CSS | Config Save | Write all configurables to SPI flash |
| CVR | Colorchord Values Read | Return all configurables in string form over UDP |
| CVW | Colorchord Values Write | Given a name and value pair, set a configurable |

Also there's a UDP server on port 7777 which can set the LEDs. Just send it an array of raw bytes for the LEDs in RGB order. So index 0 is LED1_R, index 1 is LED1_G, index 2 is LED1_B, index 3 is LED2_R, etc. It seems to ignore the first three bytes sent (first LED), but reads three bytes past where the data ends, so that may be a bug.

## Adding Modes

Adding differents modes to the swadge is easy! First fill out a ```swadgeMode``` struct as defined in ```user_main.h```. This struct contains a number of funciton pointers which will be called when that particular mode is active. Lets look at the struct in detail:
```
/**
 * A struct of all the function pointers necessary for a swadge mode. If a mode
 * does not need a particular function, say it doesn't do audio handling, it
 * is safe to set the pointer to NULL. It just won't be called.
 */
struct _swadgeMode
{
    /**
     * This swadge mode's name. Used if this mode connects to other swadges
     * over WiFi to label packets. Must be distinct for each mode.
     */
    char* modeName;
    
    /**
     * This function is called when this mode is started. It should initialize
     * any necessary variables
     */
    void (*fnEnterMode)(void);
    
    /**
     * This function is called when the mode is exited. It should clean up
     * anything that shouldn't happen when the mode is not active
     */
    void (*fnExitMode)(void);
    
    /**
     * This function is called every 100ms from user_main.c's timerFunc100ms().
     * It should be used to update any state based on time
     */
    void (*fnTimerCallback)(void);
    
    /**
     * This function is called when a button press is detected from user_main.c's
     * HandleButtonEvent(). It does not pass mode select button events. It is
     * called from an interrupt, so do the minimal amount of processing here as
     * possible. It is better to do heavy processing in timerCallback(). Any
     * global variables shared between buttonCallback() and other functions must
     * be declared volatile.
     *
     * @param state A bitmask of all button statuses
     * @param button  The button number which was pressed
     * @param down 1 if the button was pressed, 0 if it was released
     */
    void (*fnButtonCallback)(uint8_t state, int button, int down);
    
    /**
     * This function is called whenever an audio sample is read from the
     * microphone (ADC), is filtered, and is ready for processing
     *
     * @param audoSample A 32 bit audio sample
     */
    void (*fnAudioCallback)(int32_t audoSample);
    
    /**
     * This is a setting, not a function pointer. Set it to one of these
     * values to have the system configure the swadge's WiFi
     * NULL_MODE - Turn off WiFi
     * STATION_MODE - ???
     * SOFTAP_MODE - Become a WiFi access point serving up the colorchord
     *               configuration website
     * STATIONAP_MODE - Attempt to connect to the physically closest swadge
     *                  in an ad-hoc manner
     */
    uint8_t shouldConnect;
    
    /**
     * If shouldConnect is set to STATIONAP_MODE, the LED color to be used
     * during the connection process. Must be distinct for each mode.
     */
    uint32_t connectionColor;
    
    /**
     * If shouldConnect is set to STATIONAP_MODE, this function will be called
     * when the swadge connects or disconnects to another swadge. While the
     * swadge is connecting, the mode's functions will not be called. The
     * system will take over the LEDs for status purposes
     *
     * @param isConnected true if the swadge connects, false if it disconnects
     */
    void (*fnConnectionCallback)(bool isConnected);
    
    /**
     * If shouldConnect is set to STATIONAP_MODE, this function will be called
     * when a packet is received from the other swadge
     *
     * @param packet    The data to send to the connected swadge
     * @param packetLen The length of the data to send to the connected swadge
     */
    void (*fnPacketCallback)(uint8_t* packet, uint8_t packetLen);
    
    /**
     * A pointer to another swadgeMode, used by the system to register a
     * linked list of modes. This must be initialized to NULL.
     */
    swadgeMode* next;
};
```
Once the ```swadgeMode``` struct is filled out, register it with ```RegisterSwadgeMode()``` at the bottom of ```user_init()``` in ```user_main.c```. You'll also likely have to add your new source file to the ```SRCS``` makefile variable in ```user.cfg```.
