/*==============================================================================
 * Includes
 *============================================================================*/

#include <math.h>
#include <stdlib.h>
#include <osapi.h>
#include <user_interface.h>
#include "user_main.h"
#include "mode_raycaster.h"
#include "oled.h"
#include "bresenham.h"
#include "buttons.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define mapWidth 24
#define mapHeight 24

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    uint8_t wallX;
    uint8_t wallY;
    uint8_t side;
    int16_t drawStart;
    int16_t drawEnd;
} rayResult_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR raycasterEnterMode(void);
void ICACHE_FLASH_ATTR raycasterExitMode(void);
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state __attribute__((unused)),
        int32_t button, int32_t down);
void ICACHE_FLASH_ATTR raycasterProcess(void* unused);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode raycasterMode =
{
    .modeName = "raycaster",
    .fnEnterMode = raycasterEnterMode,
    .fnExitMode = raycasterExitMode,
    .fnButtonCallback = raycasterButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "copter-menu.gif"
};

timer_t raycasterTimer;
uint8_t rButtonState = 0;
int32_t worldMap[mapWidth][mapHeight] =
{
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 3, 0, 3, 0, 3, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 2, 2, 0, 2, 2, 0, 0, 0, 0, 3, 0, 3, 0, 3, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 0, 4, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 0, 0, 0, 0, 5, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 0, 4, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 0, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

float posX = 22, posY = 12;  //x and y start position
float dirX = -1, dirY = 0; //initial direction vector
float planeX = 0, planeY = 0.66; //the 2d raycaster version of camera plane

uint32_t time = 0; //time of current frame
uint32_t oldTime = 0; //time of previous frame

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * TODO
 */
void ICACHE_FLASH_ATTR raycasterEnterMode(void)
{
    enableDebounce(false);
    timerSetFn(&raycasterTimer, &raycasterProcess, NULL);
    timerArm(&raycasterTimer, 10, true);
}

/**
 * TODO
 */
void ICACHE_FLASH_ATTR raycasterExitMode(void)
{
    timerDisarm(&raycasterTimer);
    timerFlush();
}

/**
 * TODO
 *
 * @param state
 * @param button
 * @param down
 */
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state __attribute__((unused)),
        int32_t button, int32_t down)
{
    rButtonState = state;
}

/**
 * TODO
 *
 * @param unused
 */
void ICACHE_FLASH_ATTR raycasterProcess(void* unused)
{
    clearDisplay();

    rayResult_t rayResult[OLED_WIDTH] = {0};

    for(int32_t x = 0; x < OLED_WIDTH; x++)
    {
        //calculate ray position and direction
        float cameraX = 2 * x / (float)OLED_WIDTH - 1; //x-coordinate in camera space
        float rayDirX = dirX + planeX * cameraX;
        float rayDirY = dirY + planeY * cameraX;
        //which box of the map we're in
        int32_t mapX = (int32_t)(posX);
        int32_t mapY = (int32_t)(posY);

        //length of ray from current position to next x or y-side
        float sideDistX;
        float sideDistY;

        //length of ray from one x or y-side to next x or y-side
        float deltaDistX = (1 / rayDirX);
        if(deltaDistX < 0)
        {
            deltaDistX = -deltaDistX;
        }
        float deltaDistY = (1 / rayDirY);
        if(deltaDistY < 0)
        {
            deltaDistY = -deltaDistY;
        }
        float perpWallDist;

        //what direction to step in x or y-direction (either +1 or -1)
        int32_t stepX;
        int32_t stepY;

        int32_t hit = 0; //was there a wall hit?
        int32_t side; //was a NS or a EW wall hit?
        //calculate step and initial sideDist
        if(rayDirX < 0)
        {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        }
        else
        {
            stepX = 1;
            sideDistX = (mapX + 1.0 - posX) * deltaDistX;
        }
        if(rayDirY < 0)
        {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        }
        else
        {
            stepY = 1;
            sideDistY = (mapY + 1.0 - posY) * deltaDistY;
        }
        //perform DDA
        while (hit == 0)
        {
            //jump to next map square, OR in x-direction, OR in y-direction
            if(sideDistX < sideDistY)
            {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            }
            else
            {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }
            //Check if ray has hit a wall
            if(worldMap[mapX][mapY] > 0)
            {
                hit = 1;
            }
        }
        //Calculate distance projected on camera direction (Euclidean distance will give fisheye effect!)
        if(side == 0)
        {
            perpWallDist = (mapX - posX + (1 - stepX) / 2) / rayDirX;
        }
        else
        {
            perpWallDist = (mapY - posY + (1 - stepY) / 2) / rayDirY;
        }

        //Calculate height of line to draw on screen
        int32_t lineHeight = (int32_t)(OLED_HEIGHT / perpWallDist);

        //calculate lowest and highest pixel to fill in current stripe
        int32_t drawStart = -lineHeight / 2 + OLED_HEIGHT / 2;
        int32_t drawEnd = lineHeight / 2 + OLED_HEIGHT / 2;

        rayResult[x].wallX = mapX;
        rayResult[x].wallY = mapY;
        rayResult[x].side = side;
        rayResult[x].drawEnd = drawEnd;
        rayResult[x].drawStart = drawStart;
    }

    bool drawVertNext = false;
    for(int32_t x = 0; x < OLED_WIDTH; x++)
    {
        if(drawVertNext)
        {
            // This is corner or edge which is larger than the prior strip
            // Draw a vertical strip
            for(int32_t y = rayResult[x].drawStart; y <= rayResult[x].drawEnd; y++)
            {
                drawPixel(x, y, WHITE);
            }
            drawVertNext = false;
        }
        else if(x < OLED_WIDTH - 1)
        {
            if(((rayResult[x].wallX == rayResult[x + 1].wallX) ||
                    (rayResult[x].wallY == rayResult[x + 1].wallY)) &&
                    (rayResult[x].side == rayResult[x + 1].side))
            {
                // This vertical strip is part of a continuous wall with the next strip
                // Just draw top and bottom pixels
                drawPixel(x, rayResult[x].drawStart, WHITE);
                drawPixel(x, rayResult[x].drawEnd, WHITE);
            }
            else if((rayResult[x].drawEnd - rayResult[x].drawStart) >
                    (rayResult[x + 1].drawEnd - rayResult[x + 1].drawStart))
            {
                // This is a corner or edge, and this vertical strip is larger than the next one
                // Draw a vertical strip
                for(int32_t y = rayResult[x].drawStart; y <= rayResult[x].drawEnd; y++)
                {
                    drawPixel(x, y, WHITE);
                }
            }
            else
            {
                // This is a corner or edge, and this vertical strip is smaller than the next one
                // Just draw top and bottom pixels, but make sure to draw a vertical line next
                drawPixel(x, rayResult[x].drawStart, WHITE);
                drawPixel(x, rayResult[x].drawEnd, WHITE);
                // make sure to draw a vertical line next
                drawVertNext = true;
            }
        }
        else
        {
            // These are the very last pixels, nothing to compare to
            drawPixel(x, rayResult[x].drawStart, WHITE);
            drawPixel(x, rayResult[x].drawEnd, WHITE);
        }
    }

    //timing for input and FPS counter
    oldTime = time;
    time = system_get_time();
    float frameTime = (time - oldTime) / 1000000.0; //frameTime is the time this frame has taken, in seconds

    //speed modifiers
    float moveSpeed = frameTime * 5.0; //the constant value is in squares/second
    float rotSpeed = frameTime * 3.0; //the constant value is in radians/second
    //move forward if no wall in front of you
    if(rButtonState & 0x08)
    {
        if(worldMap[(int32_t)(posX + dirX * moveSpeed)][(int32_t)(posY)] == false)
        {
            posX += dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(posX)][(int32_t)(posY + dirY * moveSpeed)] == false)
        {
            posY += dirY * moveSpeed;
        }
    }
    //move backwards if no wall behind you
    if(rButtonState & 0x02)
    {
        if(worldMap[(int32_t)(posX - dirX * moveSpeed)][(int32_t)(posY)] == false)
        {
            posX -= dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(posX)][(int32_t)(posY - dirY * moveSpeed)] == false)
        {
            posY -= dirY * moveSpeed;
        }
    }
    //rotate to the right
    if(rButtonState & 0x04)
    {
        //both camera direction and camera plane must be rotated
        float oldDirX = dirX;
        dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
        dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
        float oldPlaneX = planeX;
        planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
        planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
    }
    //rotate to the left
    if(rButtonState & 0x01)
    {
        //both camera direction and camera plane must be rotated
        float oldDirX = dirX;
        dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
        dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
        float oldPlaneX = planeX;
        planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
        planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
    }
}
