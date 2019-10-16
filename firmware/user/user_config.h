#ifndef USER_CONFIG_H_
#define USER_CONFIG_H_
//#define USE_2019_SWADGE


// The accelerometer has two arrows marked x and y and dot in circle marking up
// can specify how these relate to the landscape view of OLED. In .bashrc put
// export SET_SWADGE_VERSION=1
// makefile has been changed to create -DSWADGE_VERSION=1 or 0
#define SWADGE_DEV_KIT 0
#define SWADGE_BBKIWI  1
#define SWADGE_BARREL  2
#if SWADGE_VERSION == SWADGE_BBKIWI
    //bbkiwi swadge mockup
    #define NUM_LIN_LEDS 16
    #define LED_1 15
    #define LED_2 12
    #define LED_3 10
    #define LED_4 7
    #define LED_5 4
    #define LED_6 2
    #define LEFTOLED accel.x
    #define TOPOLED (-accel.y)
    #define FACEOLED accel.z
    //swadge dev kit
    //#define LEFTOLED accel.y
    //#define TOPOLED  accel.x
    //#define FACEOLED accel.z
#elif SWADGE_VERSION == SWADGE_DEV_KIT
    #define NUM_LIN_LEDS 6
    #define LED_1 3
    #define LED_2 5
    #define LED_3 6
    #define LED_4 7
    #define LED_5 1
    #define LED_6 2
#else // SWADGE_BARREL
    #define NUM_LIN_LEDS 6
    #define LED_1 1
    #define LED_2 2
    #define LED_3 3
    #define LED_4 4
    #define LED_5 5
    #define LED_6 6
#endif

#endif