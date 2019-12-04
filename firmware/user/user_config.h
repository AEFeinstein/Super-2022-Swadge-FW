#ifndef USER_CONFIG_H_
#define USER_CONFIG_H_
// The accelerometer has two arrows marked x and y and dot in circle marking up
// can specify how these relate to the landscape view of OLED. In .bashrc put
// export SET_SWADGE_VERSION=1
// makefile has been changed to create -DSWADGE_VERSION=0,1,2 or 3

//The 2019 SWADGE can be used for testing - Its left button becomes mode change
//    its white buttom becomes left, and the right button remains right

#define SWADGE_DEV_KIT 0
#define SWADGE_BBKIWI  1
#define SWADGE_BARREL  2
#define SWADGE_2019    3
#define BARREL_1_0_0   4

#if SWADGE_VERSION == SWADGE_2019
    #define NUM_LIN_LEDS 6
    #define LED_1 1
    #define LED_2 0
    #define LED_3 5
    #define LED_4 4
    #define LED_5 3
    #define LED_6 2
#elif SWADGE_VERSION == SWADGE_BBKIWI
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
    #define NUM_LIN_LEDS 8
    #define LED_1 2
    #define LED_2 4
    #define LED_3 5
    #define LED_4 6
    #define LED_5 0
    #define LED_6 1
#elif ((SWADGE_VERSION == SWADGE_BARREL) || (SWADGE_VERSION == BARREL_1_0_0))
    #define NUM_LIN_LEDS 6
    #define LED_1 2
    #define LED_2 1
    #define LED_3 0
    #define LED_4 5
    #define LED_5 4
    #define LED_6 3
#endif

#endif
