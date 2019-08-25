/*
 * mode_snake.c
 *
 *  Created on: Jul 28, 2018
 *      Author: adam
 */

/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <mem.h>

#include "user_main.h"
#include "mode_snake.h"
#include "oled.h"
#include "font.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define SPRITE_DIM 4
#define SNAKE_FIELD_OFFSET_X 24
#define SNAKE_FIELD_OFFSET_Y 14
#define SNAKE_FIELD_WIDTH  SPRITE_DIM * 20
#define SNAKE_FIELD_HEIGHT SPRITE_DIM * 11
#define SNAKE_INITIAL_LEN 7

/*============================================================================
 * Sprites
 *==========================================================================*/

const uint8_t snakeBackground[] =
{
    0x80, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x01,
    0x80, 0xfe, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x7f, 0x01,
    0x80, 0xfe, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x7f, 0x01,
    0x80, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x01,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xfe, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0x7f, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x00, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x00,
    0x80, 0xff, 0x0e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff, 0x01,
    0x80, 0xff, 0x8e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0xff, 0x01,
    0x80, 0xff, 0x8e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0xff, 0x01,
    0x80, 0x7f, 0x8e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0xfe, 0x01,
    0x80, 0x7f, 0x8e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0xfe, 0x01,
    0x80, 0x7f, 0x8e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x71, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x7f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfe, 0x01,
    0x80, 0x3f, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x61, 0xfc, 0x01,
    0xc0, 0x3f, 0xc6, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x63, 0xfc, 0x03,
    0xc0, 0x3f, 0xc2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43, 0xfc, 0x03,
    0xc0, 0x3f, 0xc2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43, 0xfc, 0x03,
    0xc0, 0x3f, 0xc2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43, 0xfc, 0x03,
    0xc0, 0x3f, 0xc2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43, 0xfc, 0x03,
    0xc0, 0x1f, 0xe2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x47, 0xf8, 0x03,
    0xc0, 0x1f, 0xe2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x47, 0xf8, 0x03,
    0xc0, 0x1f, 0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0xf8, 0x03,
    0xc0, 0x1f, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xf8, 0x03,
    0xc0, 0x1f, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xf8, 0x03,
    0xc0, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0x03,
    0xc0, 0x0f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xf0, 0x03,
    0xc0, 0x0f, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf0, 0x03,
};

typedef enum
{
    HEAD_UP              = 0b0000011001101010,
    HEAD_RIGHT           = 0b1000011011100000,
    HEAD_DOWN            = 0b1010011001100000,
    HEAD_LEFT            = 0b0001011001110000,
    EATH_UP              = 0b0000100101101010,
    EATH_RIGHT           = 0b1010010011000010,
    EATH_DOWN            = 0b1010011010010000,
    EATH_LEFT            = 0b0101001000110100,
    BODY_UP              = 0b0110001001000110,
    BODY_RIGHT           = 0b0000110110110000,
    BODY_DOWN            = 0b0110010000100110,
    BODY_LEFT            = 0b0000101111010000,
    TAIL_UP              = 0b0110011000100010,
    TAIL_RIGHT           = 0b0000001111110000,
    TAIL_DOWN            = 0b0010001001100110,
    TAIL_LEFT            = 0b0000110011110000,
    BODY_UP_FAT          = 0b0110101111010110,
    BODY_RIGHT_FAT       = 0b0110110110110110,
    BODY_DOWN_FAT        = 0b0110110110110110,
    BODY_LEFT_FAT        = 0b0110101111010110,
    CORNER_UPRIGHT       = 0b0000001101010110,
    CORNER_UPLEFT        = 0b0000110010100110,
    CORNER_DOWNRIGHT     = 0b0110010100110000,
    CORNER_DOWNLEFT      = 0b0110101011000000,
    CORNER_RIGHTUP       = 0b0110101011000000,
    CORNER_RIGHTDOWN     = 0b0000110010100110,
    CORNER_LEFTUP        = 0b0110010100110000,
    CORNER_LEFTDOWN      = 0b0000001101010110,
    CORNER_UPRIGHT_FAT   = 0b0000001101010111,
    CORNER_UPLEFT_FAT    = 0b0000110010101110,
    CORNER_DOWNRIGHT_FAT = 0b0111010100110000,
    CORNER_DOWNLEFT_FAT  = 0b1110101011000000,
    CORNER_RIGHTUP_FAT   = 0b1110101011000000,
    CORNER_RIGHTDOWN_FAT = 0b0000110010101110,
    CORNER_LEFTUP_FAT    = 0b0111010100110000,
    CORNER_LEFTDOWN_FAT  = 0b0000001101010111,
    FOOD                 = 0b0100101001000000,
} snakeSprite;

const snakeSprite spriteTransitionTable[2][4][4] =
{
    // Snake is skinny
    {
        // Head is UP
        {
            // Body is UP
            BODY_UP,
            // Body is RIGHT
            CORNER_RIGHTUP,
            // Body is DOWN
            BODY_UP,
            // Body is LEFT
            CORNER_LEFTUP,
        },
        // Head is RIGHT
        {
            // Body is UP
            CORNER_UPRIGHT,
            // Body is RIGHT
            BODY_RIGHT,
            // Body is DOWN
            CORNER_DOWNRIGHT,
            // Body is LEFT
            BODY_RIGHT,
        },
        // Head is DOWN
        {
            // Body is UP
            BODY_DOWN,
            // Body is RIGHT
            CORNER_RIGHTDOWN,
            // Body is DOWN
            BODY_DOWN,
            // Body is LEFT
            CORNER_LEFTDOWN,
        },
        // Head is LEFT
        {
            // Body is UP
            CORNER_UPLEFT,
            // Body is RIGHT
            BODY_LEFT,
            // Body is DOWN
            CORNER_DOWNLEFT,
            // Body is LEFT
            BODY_LEFT,
        }
    },
    // Snake is fat
    {
        // Head is UP
        {
            // Body is UP
            BODY_UP_FAT,
            // Body is RIGHT
            CORNER_RIGHTUP_FAT,
            // Body is DOWN
            BODY_UP_FAT,
            // Body is LEFT
            CORNER_LEFTUP_FAT,
        },
        // Head is RIGHT
        {
            // Body is UP
            CORNER_UPRIGHT_FAT,
            // Body is RIGHT
            BODY_RIGHT_FAT,
            // Body is DOWN
            CORNER_DOWNRIGHT_FAT,
            // Body is LEFT
            BODY_RIGHT_FAT,
        },
        // Head is DOWN
        {
            // Body is UP
            BODY_DOWN_FAT,
            // Body is RIGHT
            CORNER_RIGHTDOWN_FAT,
            // Body is DOWN
            BODY_DOWN_FAT,
            // Body is LEFT
            CORNER_LEFTDOWN_FAT,
        },
        // Head is LEFT
        {
            // Body is UP
            CORNER_UPLEFT_FAT,
            // Body is RIGHT
            BODY_LEFT_FAT,
            // Body is DOWN
            CORNER_DOWNLEFT_FAT,
            // Body is LEFT
            BODY_LEFT_FAT,
        }
    }
};

const snakeSprite headTransitionTable[2][4] =
{
    {
        // Head is UP
        HEAD_UP,
        // Head is RIGHT
        HEAD_RIGHT,
        // Head is DOWN
        HEAD_DOWN,
        // Head is LEFT
        HEAD_LEFT,
    },
    {
        // Head is UP
        EATH_UP,
        // Head is RIGHT
        EATH_RIGHT,
        // Head is DOWN
        EATH_DOWN,
        // Head is LEFT
        EATH_LEFT,
    }
};

const snakeSprite tailTransitionTable[4] =
{
    // Tail is UP
    TAIL_UP,
    // Tail is RIGHT
    TAIL_RIGHT,
    // Tail is DOWN
    TAIL_DOWN,
    // Tail is LEFT
    TAIL_LEFT,
};


typedef enum
{
    bug1 = 0b10101010101010101010101010101010,
    bug2 = 0b11001100110011001100110011001100,
    bug4 = 0b11101110111011101110111011101110,
    bug3 = 0b11110000111100001111000011110000,
    bug5 = 0b11010100101010101101010010101010,
} critterSprite;

const critterSprite critterSprites[5] = {bug1, bug2, bug3, bug4, bug5};

/*============================================================================
 * Typedefs
 *==========================================================================*/

typedef struct
{
    uint8_t x;
    uint8_t y;
} pos_t;

typedef enum
{
    UP    = 0,
    RIGHT = 1,
    DOWN  = 2,
    LEFT  = 3
} dir_t;

typedef struct _snakeNode_t
{
    snakeSprite sprite;
    pos_t pos;
    dir_t dir;
    uint8_t ttl;
    uint8_t isFat;
    struct _snakeNode_t* prevSegment;
    struct _snakeNode_t* nextSegment;
} snakeNode_t;

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR snakeInit(void);
void ICACHE_FLASH_ATTR snakeDeinit(void);
void ICACHE_FLASH_ATTR snakeButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR addSnakeNode(uint8_t ttl);
void ICACHE_FLASH_ATTR drawSnakeFrame(void* arg);
void ICACHE_FLASH_ATTR plotSnakeSprite(uint8_t x, uint8_t y, snakeSprite sprite);

void ICACHE_FLASH_ATTR moveSnake(void);
void ICACHE_FLASH_ATTR drawSnake(void);
void ICACHE_FLASH_ATTR placeCritter(void);
void ICACHE_FLASH_ATTR drawCritter(void);
void ICACHE_FLASH_ATTR placeSnakeFood(void);
void ICACHE_FLASH_ATTR drawFood(void);

bool ICACHE_FLASH_ATTR isOccupiedBySnake(uint8_t x, uint8_t y, snakeNode_t* node);
inline uint8_t ICACHE_FLASH_ATTR wrapIdx(uint8_t idx, int8_t delta, uint8_t max);
void ICACHE_FLASH_ATTR moveSnakePos(pos_t* pos, dir_t dir);
uint8_t ICACHE_FLASH_ATTR isFoodAheadOfHead(void);

/*============================================================================
 * Variables
 *==========================================================================*/

swadgeMode snakeMode =
{
    .modeName = "Snake",
    .fnEnterMode = snakeInit,
    .fnExitMode = snakeDeinit,
    .fnButtonCallback = snakeButtonCallback,
    .fnAudioCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
};

struct
{
    snakeNode_t* snakeList;
    dir_t dir;
    pos_t posFood;
    pos_t posCritter;
    critterSprite cSprite;
    uint16_t length;
    uint32_t score;
    uint8_t foodEaten;
    uint16_t lastCritterAt;
    uint8_t critterTimerCount;
    os_timer_t timerHandleSnakeLogic;
} snake;

/*============================================================================
 * Functions
 *==========================================================================*/

/**
 * Initialize by zeroing out everything, buildling the snake,
 * and starting the timers
 */
void ICACHE_FLASH_ATTR snakeInit(void)
{
    // Clear everything
    ets_memset(&snake, 0, sizeof(snake));

    uint8_t i;
    for(i = 0; i < SNAKE_INITIAL_LEN; i++)
    {
        addSnakeNode(SNAKE_INITIAL_LEN - i);
    }

    snake.dir = RIGHT;
    snake.score = 0;
    snake.foodEaten = 0;
    snake.critterTimerCount = 0;

    // randomly place food
    placeSnakeFood();

    drawSnakeFrame(NULL);

    // Start a software timer to run every 400ms
    os_timer_disarm(&snake.timerHandleSnakeLogic);
    os_timer_setfn(&snake.timerHandleSnakeLogic, (os_timer_func_t*)drawSnakeFrame, NULL);
    os_timer_arm(&snake.timerHandleSnakeLogic, 400, 1);
}

/**
 * Deinitialize this by disarming all timers and freeing memory
 */
void ICACHE_FLASH_ATTR snakeDeinit(void)
{
    os_timer_disarm(&snake.timerHandleSnakeLogic);

    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        snakeNode_t* nextPtr = snakePtr->nextSegment;
        os_free(snakePtr);
        snakePtr = nextPtr;
    }
}

/**
 * Called whenever there is a button press. Changes the direction of the snake
 *
 * @param state A bitmask of all current button states
 * @param button The button ID that triggered this callback
 * @param down The state of the button that triggered this callback
 */
void ICACHE_FLASH_ATTR snakeButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    if(down)
    {
        switch(button)
        {
            case 1:
            {
                if(0 == snake.dir)
                {
                    snake.dir += 4;
                }
                snake.dir--;
                break;
            }
            case 2:
            {
                snake.dir = (snake.dir + 1) % 4;
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

/**
 * @brief Add a segment to the end of the snake
 *
 * @param The sprite of the segment to add
 */
void ICACHE_FLASH_ATTR addSnakeNode(uint8_t ttl)
{
    snake.length++;

    // If snakeList is NULL, start the snake
    if(NULL == snake.snakeList)
    {
        snake.snakeList = (snakeNode_t*)os_malloc(sizeof(snakeNode_t));
        snake.snakeList->sprite = HEAD_RIGHT;
        snake.snakeList->ttl = ttl;
        // Start in the middle of the display
        snake.snakeList->pos.x = SNAKE_FIELD_WIDTH / 2;
        snake.snakeList->pos.y = SNAKE_FIELD_HEIGHT / 2 + 2;
        snake.snakeList->dir = RIGHT;
        snake.snakeList->isFat = 0;
        snake.snakeList->prevSegment = NULL;
        snake.snakeList->nextSegment = NULL;
        return;
    }

    // Iterate through the list, and tack on a new segment
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr->nextSegment)
    {
        snakePtr = snakePtr->nextSegment;
    }
    snakePtr->nextSegment = (snakeNode_t*)os_malloc(sizeof(snakeNode_t));
    snakePtr->nextSegment->ttl = ttl;
    if(1 == ttl)
    {
        snakePtr->nextSegment->sprite = TAIL_RIGHT;
    }
    else
    {
        snakePtr->nextSegment->sprite = BODY_RIGHT;
    }
    snakePtr->nextSegment->pos.x = snakePtr->pos.x - SPRITE_DIM;
    snakePtr->nextSegment->pos.y = snakePtr->pos.y;
    snakePtr->nextSegment->dir = RIGHT;
    snakePtr->nextSegment->isFat = 0;
    snakePtr->nextSegment->prevSegment = snakePtr;
    snakePtr->nextSegment->nextSegment = NULL;
}

/**
 * Move the snake's position, check game logic, then draw a frame
 */
void ICACHE_FLASH_ATTR drawSnakeFrame(void* arg __attribute__((unused)))
{
    char scoreStr[16];
    // TODO check game logic (check death, check food, grow snake, randomly place food)
    // TODO keep track of score, display score on death

    clearDisplay();

    // TODO use drawFrame(snakeBackground) instead
    for(int y = 0; y < OLED_HEIGHT; y++)
    {
        for(int x = 0; x < OLED_WIDTH; x++)
        {
            if(snakeBackground[(y * (OLED_WIDTH / 8)) + (x / 8)] & (0x80 >> (x % 8)))
            {
                drawPixel(x, y, BLACK);
            }
            else
            {
                drawPixel(x, y, WHITE);
            }
        }
    }

    if(snake.critterTimerCount > 0)
    {
        snake.critterTimerCount--;
    }

    moveSnake();
    drawSnake();

    if(snake.critterTimerCount > 0)
    {
        drawCritter();
        ets_snprintf(scoreStr, sizeof(scoreStr), "%02d", snake.critterTimerCount);
        plotText(96, 5, scoreStr, TOM_THUMB);
    }
    drawFood();

    ets_snprintf(scoreStr, sizeof(scoreStr), "%04d", snake.score);
    plotText(24, 5, scoreStr, TOM_THUMB);

}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR moveSnake(void)
{
    // Save the old head
    snakeNode_t* oldHead = snake.snakeList;

    // create a new head and link it to the list
    snakeNode_t* newHead = (snakeNode_t*)os_malloc(sizeof(snakeNode_t));
    newHead->prevSegment = NULL;
    newHead->nextSegment = oldHead;
    oldHead->prevSegment = newHead;
    snake.snakeList = newHead;

    // Figure out where the new head is, and its sprite
    newHead->dir = snake.dir;
    newHead->pos.x = newHead->nextSegment->pos.x;
    newHead->pos.y = newHead->nextSegment->pos.y;
    moveSnakePos(&newHead->pos, newHead->dir);
    newHead->ttl = snake.length;

    // Figure out the sprite based on the food location and direction
    newHead->sprite = headTransitionTable[isFoodAheadOfHead()][newHead->dir];

    // See if we ate anything
    newHead->isFat = 0;
    if(newHead->pos.x == snake.posFood.x && newHead->pos.y == snake.posFood.y)
    {
        // Mmm, tasty
        newHead->isFat = 1;
        snake.foodEaten++;

        // Increment all the ttls, effectively making the snake longer
        snake.length++;
        snakeNode_t* snakePtr = snake.snakeList;
        while(NULL != snakePtr)
        {
            snakePtr->ttl++;
            snakePtr = snakePtr->nextSegment;
        }

        snake.score += 5;
    }
    else if((newHead->pos.y == snake.posCritter.y) && (newHead->pos.x == snake.posCritter.x
            || newHead->pos.x == snake.posCritter.x + SPRITE_DIM) )
    {
        // Mmm a critter
        newHead->isFat = 1;

        // Increment all the ttls, effectively making the snake longer
        snake.length++;
        snakeNode_t* snakePtr = snake.snakeList;
        while(NULL != snakePtr)
        {
            snakePtr->ttl++;
            snakePtr = snakePtr->nextSegment;
        }

        snake.score += (5 * snake.critterTimerCount);

        snake.critterTimerCount = 0;
    }

    // Swap sprite right behind the head and adjust it's direction to match the head
    snakeNode_t* neck = newHead->nextSegment;
    newHead->nextSegment->sprite = spriteTransitionTable[newHead->nextSegment->isFat][newHead->dir][neck->nextSegment->dir];
    neck->dir = newHead->dir;

    // Iterate through the list, decrementing the ttls
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        snakePtr->ttl--;
        if(1 == snakePtr->ttl)
        {
            // Draw a new tail here
            snakePtr->sprite = tailTransitionTable[snakePtr->dir];
        }
        else if(0 == snakePtr->ttl)
        {
            // If this segment is done, remove it from the list and free it
            snakePtr->prevSegment->nextSegment = NULL;
            os_free(snakePtr);
            snakePtr = NULL;
        }

        // iterate
        if (NULL != snakePtr)
        {
            snakePtr = snakePtr->nextSegment;
        }
    }

    if(isOccupiedBySnake(newHead->pos.x, newHead->pos.y, snake.snakeList->nextSegment))
    {
        // Collided with self, game over
        os_timer_disarm(&snake.timerHandleSnakeLogic);
    }
    else
    {
        // If the food was eaten, place it somewhere else
        if(newHead->isFat)
        {
            placeSnakeFood();
        }
        if(0 == snake.critterTimerCount &&
                snake.lastCritterAt != snake.foodEaten &&
                snake.foodEaten % 5 == 0)
        {
            // Pick random snake.cSprite
            snake.cSprite = critterSprites[os_random() % (sizeof(critterSprites) / sizeof(critterSprites[0]))];
            snake.critterTimerCount = 20;
            placeCritter();
            snake.lastCritterAt = snake.foodEaten;
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR placeSnakeFood(void)
{
    uint16_t randPos = os_random() % (((SNAKE_FIELD_HEIGHT / SPRITE_DIM) *
                                       (SNAKE_FIELD_WIDTH  / SPRITE_DIM)) -
                                      snake.length);
    uint16_t linearAddr = 0;
    for(uint8_t y = 0; y < SNAKE_FIELD_HEIGHT; y += SPRITE_DIM)
    {
        for(uint8_t x = 0; x < SNAKE_FIELD_WIDTH; x += SPRITE_DIM)
        {
            if(!isOccupiedBySnake(x, y, snake.snakeList))
            {
                if(randPos == linearAddr)
                {
                    snake.posFood.x = x;
                    snake.posFood.y = y;
                    return;
                }
                else
                {
                    linearAddr++;
                }
            }
        }
    }
    // TODO ultimate winner?
}

/**
 * @brief
 *
 */
void ICACHE_FLASH_ATTR placeCritter(void)
{
    uint16_t randPos = os_random() % (((SNAKE_FIELD_HEIGHT / SPRITE_DIM) *
                                       ((SNAKE_FIELD_WIDTH - SPRITE_DIM)  / SPRITE_DIM)) -
                                      snake.length);
    uint16_t linearAddr = 0;
    for(uint8_t y = 0; y < SNAKE_FIELD_HEIGHT; y += SPRITE_DIM)
    {
        for(uint8_t x = 0; x < SNAKE_FIELD_WIDTH - SPRITE_DIM; x += SPRITE_DIM)
        {
            if(!isOccupiedBySnake(x, y, snake.snakeList) && !isOccupiedBySnake(x + SPRITE_DIM, y, snake.snakeList))
            {
                if(randPos == linearAddr)
                {
                    snake.posCritter.x = x;
                    snake.posCritter.y = y;
                    return;
                }
                else
                {
                    linearAddr++;
                }
            }
        }
    }
    // TODO ultimate winner?
}

/*******************************************************************************
 * Helper functions
 ******************************************************************************/

/**
 * @brief TODO
 *
 * @param idx
 * @param delta
 * @param min
 * @param max
 * @return uint8_t wrapIdx
 */
inline uint8_t ICACHE_FLASH_ATTR wrapIdx(uint8_t idx, int8_t delta, uint8_t max)
{
    if(idx + delta < 0)
    {
        idx += max;
    }
    idx += delta;
    if(idx >= max)
    {
        idx -= max;
    }
    return idx;
}

/**
 * @brief TODO
 *
 * @param pos
 * @param dir
 */
void ICACHE_FLASH_ATTR moveSnakePos(pos_t* pos, dir_t dir)
{
    switch(dir)
    {
        case UP:
        {
            pos->y = wrapIdx(pos->y, -SPRITE_DIM, SNAKE_FIELD_HEIGHT);
            break;
        }
        case DOWN:
        {
            pos->y = wrapIdx(pos->y, SPRITE_DIM, SNAKE_FIELD_HEIGHT);
            break;
        }
        case LEFT:
        {
            pos->x = wrapIdx(pos->x, -SPRITE_DIM, SNAKE_FIELD_WIDTH);
            break;
        }
        case RIGHT:
        {
            pos->x = wrapIdx(pos->x, SPRITE_DIM, SNAKE_FIELD_WIDTH);
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief TODO
 *
 * @param toCheck
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR isOccupiedBySnake(uint8_t x, uint8_t y, snakeNode_t* node)
{
    while(NULL != node)
    {
        if((node->pos.x == x) && (node->pos.y == y))
        {
            return true;
        }
        node = node->nextSegment;
    }
    return false;
}

/**
 * @brief TODO
 *
 * @return uint8_t
 */
uint8_t ICACHE_FLASH_ATTR isFoodAheadOfHead(void)
{
    pos_t headPos;
    headPos.x = snake.snakeList->pos.x;
    headPos.y = snake.snakeList->pos.y;
    moveSnakePos(&headPos, snake.snakeList->dir);
    if(headPos.x == snake.posFood.x && headPos.y == snake.posFood.y)
    {
        return 1;
    }
    return 0;
}

/*******************************************************************************
 * Functions for drawing
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawSnake(void)
{
    // Draw the snake
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        plotSnakeSprite(snakePtr->pos.x, snakePtr->pos.y, snakePtr->sprite);
        snakePtr = snakePtr->nextSegment;
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawFood(void)
{
    // Draw the food
    plotSnakeSprite(snake.posFood.x, snake.posFood.y, FOOD);
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawCritter(void)
{
    plotSnakeSprite(snake.posCritter.x, snake.posCritter.y,              (snake.cSprite & 0xFFFF0000) >> 16);
    plotSnakeSprite(snake.posCritter.x + SPRITE_DIM, snake.posCritter.y, (snake.cSprite & 0x0000FFFF));
}

/**
 * @brief TODO
 *
 * @param x
 * @param y
 * @param sprite
 */
void ICACHE_FLASH_ATTR plotSnakeSprite(uint8_t x, uint8_t y, snakeSprite sprite)
{
    x += SNAKE_FIELD_OFFSET_X;
    y += SNAKE_FIELD_OFFSET_Y;

    uint8_t xDraw, yDraw, spriteIdx = 15;
    for(yDraw = 0; yDraw < SPRITE_DIM; yDraw++)
    {
        for(xDraw = 0; xDraw < SPRITE_DIM; xDraw++)
        {
            if(sprite & (1 << (spriteIdx--)))
            {
                drawPixel(x + xDraw, y + yDraw, WHITE);
            }
            else
            {
                drawPixel(x + xDraw, y + yDraw, BLACK);
            }
        }
    }
}
