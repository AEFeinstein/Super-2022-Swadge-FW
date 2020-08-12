#include <math.h>
#include <stdlib.h>
#include <osapi.h>
#include <user_interface.h>
#include "user_main.h"
#include "mode_raycaster.h"
#include "oled.h"
#include "bresenham.h"
#include "buttons.h"

void ICACHE_FLASH_ATTR raycasterEnterMode(void);
void ICACHE_FLASH_ATTR raycasterExitMode(void);
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down);

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
void ICACHE_FLASH_ATTR raycasterProcess(void* unused);

void ICACHE_FLASH_ATTR raycasterEnterMode(void)
{
    enableDebounce(false);
    timerSetFn(&raycasterTimer, &raycasterProcess, NULL);
    timerArm(&raycasterTimer, 20, true);
}

void ICACHE_FLASH_ATTR raycasterExitMode(void)
{
    timerDisarm(&raycasterTimer);
    timerFlush();
}

uint8_t rButtonState = 0;

void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    rButtonState = state;
}

#define mapWidth 24
#define mapHeight 24

int worldMap[mapWidth][mapHeight] =
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

double posX = 22, posY = 12;  //x and y start position
double dirX = -1, dirY = 0; //initial direction vector
double planeX = 0, planeY = 0.66; //the 2d raycaster version of camera plane

uint32_t time = 0; //time of current frame
uint32_t oldTime = 0; //time of previous frame

void ICACHE_FLASH_ATTR raycasterProcess(void* unused)
{
    clearDisplay();
    int lastMapX = -1;
    int lastMapY = -1;
    int lastDrawStart = -1;
    int lastDrawEnd = -1;


    for(int x = 0; x < OLED_WIDTH; x++)
    {
        //calculate ray position and direction
        double cameraX = 2 * x / (double)OLED_WIDTH - 1; //x-coordinate in camera space
        double rayDirX = dirX + planeX * cameraX;
        double rayDirY = dirY + planeY * cameraX;
        //which box of the map we're in
        int mapX = (int)(posX);
        int mapY = (int)(posY);

        //length of ray from current position to next x or y-side
        double sideDistX;
        double sideDistY;

        //length of ray from one x or y-side to next x or y-side
        double deltaDistX = abs(1 / rayDirX);
        double deltaDistY = abs(1 / rayDirY);
        double perpWallDist;

        //what direction to step in x or y-direction (either +1 or -1)
        int stepX;
        int stepY;

        int hit = 0; //was there a wall hit?
        int side; //was a NS or a EW wall hit?
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
        int lineHeight = (int)(OLED_HEIGHT / perpWallDist);

        //calculate lowest and highest pixel to fill in current stripe
        int drawStart = -lineHeight / 2 + OLED_HEIGHT / 2;
        if(drawStart < 0)
        {
            drawStart = 0;
        }
        int drawEnd = lineHeight / 2 + OLED_HEIGHT / 2;
        if(drawEnd >= OLED_HEIGHT)
        {
            drawEnd = OLED_HEIGHT - 1;
        }

        if((-1 != lastMapX && -1 != lastMapY) && (lastMapX != mapX || lastMapY != mapY))
        {
            for(int y = drawStart; y <= drawEnd; y++)
            {
                drawPixel(x, y, WHITE);
            }
            if(x > 0 && -1 != lastDrawEnd)
            {
                for(int y = lastDrawStart; y <= lastDrawEnd; y++)
                {
                    drawPixel(x - 1, y, WHITE);
                }
            }
        }
        else
        {
            drawPixel(x, drawStart, WHITE);
            drawPixel(x, drawEnd, WHITE);
        }

        lastMapX = mapX;
        lastMapY = mapY;
        lastDrawStart = drawStart;
        lastDrawEnd = drawEnd;

        // //give x and y sides different brightness
        // if(side == 1)
        // {
        //     color = color / 2;
        // }

        //     for(int y = drawStart; y <= drawEnd; y++)
        //     {
        //         if(side == 1)
        //         {
        //             if(x % 2 == 0)
        //             {
        //                 if((y % 2 == 0))
        //                 {
        //                     drawPixel(x, y, WHITE);
        //                 }
        //             }
        //             else
        //             {
        //                 if((y % 2 == 1))
        //                 {
        //                     drawPixel(x, y, WHITE);
        //                 }
        //             }
        //         }
        //         else
        //         {
        //             drawPixel(x, y, WHITE);
        //         }
        //     }
        // }
    }
    //timing for input and FPS counter
    oldTime = time;
    time = system_get_time();
    double frameTime = (time - oldTime) / 1000000.0; //frameTime is the time this frame has taken, in seconds

    //speed modifiers
    double moveSpeed = frameTime * 5.0; //the constant value is in squares/second
    double rotSpeed = frameTime * 3.0; //the constant value is in radians/second
    //move forward if no wall in front of you
    if(rButtonState & 0x08)
    {
        if(worldMap[(int)(posX + dirX * moveSpeed)][(int)(posY)] == false)
        {
            posX += dirX * moveSpeed;
        }
        if(worldMap[(int)(posX)][(int)(posY + dirY * moveSpeed)] == false)
        {
            posY += dirY * moveSpeed;
        }
    }
    //move backwards if no wall behind you
    if(rButtonState & 0x02)
    {
        if(worldMap[(int)(posX - dirX * moveSpeed)][(int)(posY)] == false)
        {
            posX -= dirX * moveSpeed;
        }
        if(worldMap[(int)(posX)][(int)(posY - dirY * moveSpeed)] == false)
        {
            posY -= dirY * moveSpeed;
        }
    }
    //rotate to the right
    if(rButtonState & 0x04)
    {
        //both camera direction and camera plane must be rotated
        double oldDirX = dirX;
        dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
        dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
        double oldPlaneX = planeX;
        planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
        planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
    }
    //rotate to the left
    if(rButtonState & 0x01)
    {
        //both camera direction and camera plane must be rotated
        double oldDirX = dirX;
        dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
        dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
        double oldPlaneX = planeX;
        planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
        planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
    }
}
