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
#include "sprite.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define SPRITE_DIM 4

/*============================================================================
 * Function prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR snakeInit(void);
void ICACHE_FLASH_ATTR snakeDeinit(void);
void ICACHE_FLASH_ATTR snakeButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR addSnakeNode(const sprite_t* sprite);
void ICACHE_FLASH_ATTR drawSnakeFrame(void* arg);

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

const sprite_t spriteHead =
{
    .width = SPRITE_DIM,
    .height = SPRITE_DIM,
    .data =
    {
        0b0110,
        0b1001,
        0b1001,
        0b0110,
    }
};

const sprite_t spriteBody =
{
    .width = SPRITE_DIM,
    .height = SPRITE_DIM,
    .data =
    {
        0b0110,
        0b1111,
        0b1111,
        0b0110,
    }
};

const sprite_t spriteTail =
{
    .width = SPRITE_DIM,
    .height = SPRITE_DIM,
    .data =
    {
        0b0110,
        0b1101,
        0b1011,
        0b0110,
    }
};

const sprite_t spriteFood =
{
    .width = SPRITE_DIM,
    .height = SPRITE_DIM,
    .data =
    {
        0b1001,
        0b0110,
        0b0110,
        0b1001,
    }
};

typedef struct
{
    uint8_t x;
    uint8_t y;
} pos_t;

typedef struct _snakeNode_t
{
    const sprite_t* sprite;
    pos_t pos;
    struct _snakeNode_t* prevSegment;
    struct _snakeNode_t* nextSegment;
} snakeNode_t;

typedef enum
{
    UP,
    RIGHT,
    DOWN,
    LEFT
} dir_t;

struct
{
    snakeNode_t* snakeList;
    dir_t dirSnake;
    pos_t posFood;
} snake;

static os_timer_t timerHandleSnakeLogic = {0};

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

    addSnakeNode(&spriteHead);
    addSnakeNode(&spriteBody);
    addSnakeNode(&spriteTail);

    snake.dirSnake = RIGHT;

    // TODO randomly place food

    drawSnakeFrame(NULL);

    // Start a software timer to run every 100ms
    os_timer_disarm(&timerHandleSnakeLogic);
    os_timer_setfn(&timerHandleSnakeLogic, (os_timer_func_t*)drawSnakeFrame, NULL);
    os_timer_arm(&timerHandleSnakeLogic, 100, 1);
}

/**
 * Deinitialize this by disarming all timers and freeing memory
 */
void ICACHE_FLASH_ATTR snakeDeinit(void)
{
    os_timer_disarm(&timerHandleSnakeLogic);

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
                if(0 == snake.dirSnake)
                {
                    snake.dirSnake += 4;
                }
                snake.dirSnake--;
                break;
            }
            case 2:
            {
                snake.dirSnake = (snake.dirSnake + 1) % 4;
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
void ICACHE_FLASH_ATTR addSnakeNode(const sprite_t* sprite)
{
    // If snakeList is NULL, start the snake
    if(NULL == snake.snakeList)
    {
        snake.snakeList = (snakeNode_t*)os_malloc(sizeof(snakeNode_t));
        snake.snakeList->sprite = sprite;
        // Start in the middle of the display
        snake.snakeList->pos.x = OLED_WIDTH / 2;
        snake.snakeList->pos.y = OLED_HEIGHT / 2;
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
    snakePtr->nextSegment->sprite = sprite;
    snakePtr->nextSegment->prevSegment = snakePtr;
    snakePtr->nextSegment->nextSegment = NULL;
    snakePtr->nextSegment->pos.x = snakePtr->pos.x - 4;
    snakePtr->nextSegment->pos.y = snakePtr->pos.y;
}

/**
 * Move the snake's position, check game logic, then draw a frame
 */
void ICACHE_FLASH_ATTR drawSnakeFrame(void* arg __attribute__((unused)))
{
    // TODO check game logic (check death, check food, grow snake, randomly place food)
    // TODO keep track of score, display score on death

    clearDisplay();

    // Find the tail of the snake
    snakeNode_t* snakePtr = snake.snakeList;
    while(NULL != snakePtr->nextSegment)
    {
        snakePtr = snakePtr->nextSegment;
    }

    // Move each segment to where the previous one was
    while(NULL != snakePtr->prevSegment)
    {
        snakePtr->pos.x = snakePtr->prevSegment->pos.x;
        snakePtr->pos.y = snakePtr->prevSegment->pos.y;
        snakePtr = snakePtr->prevSegment;
    }

    // Move the snake's head
    switch(snake.dirSnake)
    {
        case UP:
        {
            if(snake.snakeList->pos.y < SPRITE_DIM)
            {
                snake.snakeList->pos.y += OLED_HEIGHT;
            }
            snake.snakeList->pos.y -= SPRITE_DIM;
            break;
        }
        case DOWN:
        {
            snake.snakeList->pos.y = (snake.snakeList->pos.y + SPRITE_DIM) % OLED_HEIGHT;
            break;
        }
        case LEFT:
        {
            if(snake.snakeList->pos.x < SPRITE_DIM)
            {
                snake.snakeList->pos.x += OLED_WIDTH;
            }
            snake.snakeList->pos.x -= SPRITE_DIM;
            break;
        }
        case RIGHT:
        {
            snake.snakeList->pos.x = (snake.snakeList->pos.x + SPRITE_DIM) % OLED_WIDTH;
            break;
        }
        default:
            break;
    }

    // Draw the snake
    snakePtr = snake.snakeList;
    while(NULL != snakePtr)
    {
        plotSprite(snakePtr->pos.x, snakePtr->pos.y, snakePtr->sprite);
        snakePtr = snakePtr->nextSegment;
    }

    // Draw the food
    plotSprite(snake.posFood.x, snake.posFood.y, &spriteFood);
}