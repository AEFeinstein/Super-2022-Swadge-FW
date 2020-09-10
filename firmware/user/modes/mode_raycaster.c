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
#include "assets.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define mapWidth 24
#define mapHeight 24

#define texWidth 32
#define texHeight 32

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    uint8_t mapX;
    uint8_t mapY;
    uint8_t side;
    int16_t drawStart;
    int16_t drawEnd;
    float perpWallDist;
    float rayDirX;
    float rayDirY;
} rayResult_t;

typedef struct
{
    float x;
    float y;
    int32_t texture;
} raySprite_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR raycasterEnterMode(void);
void ICACHE_FLASH_ATTR raycasterExitMode(void);
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state, int32_t button, int32_t down);
void ICACHE_FLASH_ATTR raycasterProcess(void);
void ICACHE_FLASH_ATTR sortSprites(int32_t* order, float* dist, int32_t amount);

void ICACHE_FLASH_ATTR castRays(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawTextures(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawOutlines(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawSprites(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR handleRayInput(void);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode raycasterMode =
{
    .modeName = "raycaster",
    .fnEnterMode = raycasterEnterMode,
    .fnExitMode = raycasterExitMode,
    .fnProcTask = raycasterProcess,
    .fnButtonCallback = raycasterButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "ray-menu.gif"
};

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

uint8_t textures[2][texWidth][texHeight];

float posX = 22, posY = 12;  // x and y start position
float dirX = -1, dirY = 0; // initial direction vector
float planeX = 0, planeY = 0.66; // the 2d raycaster version of camera plane

raySprite_t sprite[] =
{
    {20.5, 11.5, 10},
    {18.5, 4.5, 10},
    {10.0, 4.5, 10},
    {10.0, 12.5, 10},
    {3.5, 6.5, 10},
    {3.5, 20.5, 10},
    {3.5, 14.5, 10},
    {14.5, 20.5, 10},
    {18.5, 10.5, 9},
    {18.5, 11.5, 9},
    {18.5, 12.5, 9},
    {21.5, 1.5, 8},
    {15.5, 1.5, 8},
    {16.0, 1.8, 8},
    {16.2, 1.2, 8},
    {3.5,  2.5, 8},
    {9.5, 15.5, 8},
    {10.0, 15.1, 8},
    {10.5, 15.8, 8},
};

// arrays used to sort the sprites
int32_t spriteOrder[sizeof(sprite) / sizeof(sprite[0])];
float spriteDistance[sizeof(sprite) / sizeof(sprite[0])];

// Storage for the enemy sprite
color enemySpriteTex[texWidth * texHeight];

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Set up the raycaster by creating some simple textures and loading other
 * textures from assets
 */
void ICACHE_FLASH_ATTR raycasterEnterMode(void)
{
    enableDebounce(false);

    // Create simple textures
    for(uint8_t h = 0; h < texHeight; h++)
    {
        for(uint8_t w = 0; w < texWidth; w++)
        {
            // This draws an X
            if(w == h || w + 1 == h || w == (texWidth - h - 1) || w == (texWidth - h - 2))
            {
                textures[0][w][h] = 1;
            }

            // This draws a horizontal stripe
            if(texHeight / 2 - 4 < h && h < texHeight / 2 + 4)
            {
                textures[1][w][h] = 1;
            }
        }
    }

    // Load the enemy texture to RAM
    pngHandle enemySprite;
    allocPngAsset("enemy.png", &enemySprite);
    drawPngToBuffer(&enemySprite, enemySpriteTex);
    freePngAsset(&enemySprite);
}

/**
 * Free all resources allocated in raycasterEnterMode
 */
void ICACHE_FLASH_ATTR raycasterExitMode(void)
{
}

/**
 * Simple button callback which saves the state of all buttons
 *
 * @param state  A bitmask with all the current button states
 * @param button The button that caused this interrupt
 * @param down   true if the button was pushed, false if it was released
 */
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state,
        int32_t button __attribute__((unused)),
        int32_t down __attribute__((unused)))
{
    rButtonState = state;
}

/**
 * This function renders the scene and handles input. It is called as fast as
 * possible by user_main.c's procTask.
 */
void ICACHE_FLASH_ATTR raycasterProcess(void)
{
    rayResult_t rayResult[OLED_WIDTH] = {{0}};

    clearDisplay();
    castRays(rayResult);
    drawTextures(rayResult);
    drawOutlines(rayResult);
    drawSprites(rayResult);
    handleRayInput();
}

/**
 * Cast all the rays into the scene, iterating across the X axis, and save the
 * results in the rayResult argument
 *
 * @param rayResult A pointer to an array of rayResult_t where this scene's
 *                  information is stored
 */
void ICACHE_FLASH_ATTR castRays(rayResult_t* rayResult)
{
    for(int32_t x = 0; x < OLED_WIDTH; x++)
    {
        // calculate ray position and direction
        // x-coordinate in camera space
        float cameraX = 2 * x / (float)OLED_WIDTH - 1;
        float rayDirX = dirX + planeX * cameraX;
        float rayDirY = dirY + planeY * cameraX;

        // which box of the map we're in
        int32_t mapX = (int32_t)(posX);
        int32_t mapY = (int32_t)(posY);

        // length of ray from current position to next x or y-side
        float sideDistX;
        float sideDistY;

        // length of ray from one x or y-side to next x or y-side
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

        // what direction to step in x or y-direction (either +1 or -1)
        int32_t stepX;
        int32_t stepY;

        int32_t hit = 0; // was there a wall hit?
        int32_t side; // was a NS or a EW wall hit?
        // calculate step and initial sideDist
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

        // perform DDA
        while (hit == 0)
        {
            // jump to next map square, OR in x-direction, OR in y-direction
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

            // Check if ray has hit a wall
            if(worldMap[mapX][mapY] > 0)
            {
                hit = 1;
            }
        }

        // Calculate distance projected on camera direction
        // (Euclidean distance will give fisheye effect!)
        float perpWallDist;
        if(side == 0)
        {
            perpWallDist = (mapX - posX + (1 - stepX) / 2) / rayDirX;
        }
        else
        {
            perpWallDist = (mapY - posY + (1 - stepY) / 2) / rayDirY;
        }

        // Calculate height of line to draw on screen
        int32_t lineHeight = (int32_t)(OLED_HEIGHT / perpWallDist);

        // calculate lowest and highest pixel to fill in current stripe
        int32_t drawStart = -lineHeight / 2 + OLED_HEIGHT / 2;
        int32_t drawEnd = lineHeight / 2 + OLED_HEIGHT / 2;

        // Because of how drawStart and drawEnd are calculated, lineHeight needs to be recomputed
        lineHeight = drawEnd - drawStart;

        // Save a bunch of data to render the scene later
        rayResult[x].mapX = mapX;
        rayResult[x].mapY = mapY;
        rayResult[x].side = side;
        rayResult[x].drawEnd = drawEnd;
        rayResult[x].drawStart = drawStart;
        rayResult[x].perpWallDist = perpWallDist;
        rayResult[x].rayDirX = rayDirX;
        rayResult[x].rayDirY = rayDirY;
    }
}

/**
 * With the data in rayResult, render all the wall textures to the scene
 *
 * @param rayResult The information for all the rays cast
 */
void ICACHE_FLASH_ATTR drawTextures(rayResult_t* rayResult)
{
    for(int32_t x = 0; x < OLED_WIDTH; x++)
    {
        // For convenience
        uint8_t mapX      = rayResult[x].mapX;
        uint8_t mapY      = rayResult[x].mapY;
        uint8_t side      = rayResult[x].side;
        int16_t drawStart = rayResult[x].drawStart;
        int16_t drawEnd   = rayResult[x].drawEnd;

        // Make sure not to waste any draws out-of-bounds
        if(drawStart < 0)
        {
            drawStart = 0;
        }
        else if(drawStart >= OLED_HEIGHT)
        {
            drawStart = OLED_HEIGHT - 1;
        }

        if(drawEnd < 0)
        {
            drawEnd = 0;
        }
        else if(drawEnd >= OLED_HEIGHT)
        {
            drawEnd = OLED_HEIGHT - 1;
        }

        // Pick a texture
        int32_t texNum = (worldMap[mapX][mapY] - 1) % 2;

        // calculate value of wallX, where exactly the wall was hit
        float wallX;
        if(side == 0)
        {
            wallX = posY + rayResult[x].perpWallDist * rayResult[x].rayDirY;
        }
        else
        {
            wallX = posX + rayResult[x].perpWallDist * rayResult[x].rayDirX;
        }
        wallX -= (int32_t)(wallX);

        // X coordinate on the texture. Make sure it's in bounds
        int32_t texX = (int32_t)(wallX * texWidth);
        if(texX >= texWidth)
        {
            texX = texWidth - 1;
        }

        // Draw this texture's vertical stripe
        // Calculate how much to increase the texture coordinate per screen pixel
        int32_t lineHeight = rayResult[x].drawEnd - rayResult[x].drawStart;
        float step = texHeight / (float)lineHeight;
        // Starting texture coordinate
        float texPos = (drawStart - OLED_HEIGHT / 2 + lineHeight / 2) * step;
        for(int32_t y = drawStart; y < drawEnd; y++)
        {
            // Y coordinate on the texture. Round it, make sure it's in bounds
            int32_t texY = (int32_t)(texPos);
            if(texY >= texHeight)
            {
                texY = texHeight - 1;
            }

            // Increment the texture position by the step size
            texPos += step;

            // Draw the pixel specified by the texture
            drawPixel(x, y, textures[texNum][texX][texY]);
        }
    }
}

/**
 * Draw the outlines of all the walls and corners based on the cast ray info
 *
 * @param rayResult The information for all the rays cast
 */
void ICACHE_FLASH_ATTR drawOutlines(rayResult_t* rayResult)
{
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
            if(((rayResult[x].mapX == rayResult[x + 1].mapX) ||
                    (rayResult[x].mapY == rayResult[x + 1].mapY)) &&
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
}

/**
 * Draw all the sprites
 *
 * @param rayResult The information for all the rays cast
 */
void ICACHE_FLASH_ATTR drawSprites(rayResult_t* rayResult)
{
    // sort sprites from far to close
    for(uint32_t i = 0; i < sizeof(sprite) / sizeof(sprite[0]); i++)
    {
        spriteOrder[i] = i;
        // sqrt not taken, unneeded
        spriteDistance[i] = ((posX - sprite[i].x) * (posX - sprite[i].x) +
                             (posY - sprite[i].y) * (posY - sprite[i].y));
    }
    sortSprites(spriteOrder, spriteDistance, sizeof(sprite) / sizeof(sprite[0]));

    // after sorting the sprites, do the projection and draw them
    for(uint32_t i = 0; i < sizeof(sprite) / sizeof(sprite[0]); i++)
    {
        // translate sprite position to relative to camera
        float spriteX = sprite[spriteOrder[i]].x - posX;
        float spriteY = sprite[spriteOrder[i]].y - posY;

        // transform sprite with the inverse camera matrix
        // [ planeX dirX ] -1                                  [ dirY     -dirX ]
        // [             ]    =  1/(planeX*dirY-dirX*planeY) * [                ]
        // [ planeY dirY ]                                     [ -planeY planeX ]

        // required for correct matrix multiplication
        float invDet = 1.0 / (planeX * dirY - dirX * planeY);

        float transformX = invDet * (dirY * spriteX - dirX * spriteY);
        // this is actually the depth inside the screen, that what Z is in 3D
        float transformY = invDet * (-planeY * spriteX + planeX * spriteY);

        int32_t spriteScreenX = (int32_t)((OLED_WIDTH / 2) * (1 + transformX / transformY));

        // calculate height of the sprite on screen
        // using 'transformY' instead of the real distance prevents fisheye
        int32_t spriteHeight = (int32_t)(OLED_HEIGHT / (transformY));
        if(spriteHeight < 0)
        {
            spriteHeight = -spriteHeight;
        }

        // calculate lowest and highest pixel to fill in current stripe
        int32_t drawStartY = -spriteHeight / 2 + OLED_HEIGHT / 2;
        if(drawStartY < 0)
        {
            drawStartY = 0;
        }

        int32_t drawEndY = spriteHeight / 2 + OLED_HEIGHT / 2;
        if(drawEndY >= OLED_HEIGHT)
        {
            drawEndY = OLED_HEIGHT - 1;
        }

        // calculate width of the sprite
        int32_t spriteWidth = ( (int32_t) (OLED_HEIGHT / (transformY)));
        if(spriteWidth < 0)
        {
            spriteWidth = -spriteWidth;
        }
        int32_t drawStartX = -spriteWidth / 2 + spriteScreenX;
        if(drawStartX < 0)
        {
            drawStartX = 0;
        }
        int32_t drawEndX = spriteWidth / 2 + spriteScreenX;
        if(drawEndX >= OLED_WIDTH)
        {
            drawEndX = OLED_WIDTH - 1;
        }

        // loop through every vertical stripe of the sprite on screen
        for(int32_t stripe = drawStartX; stripe < drawEndX; stripe++)
        {
            int32_t texX = (int32_t)(256 * (stripe - (-spriteWidth / 2 + spriteScreenX)) * texWidth / spriteWidth) / 256;
            // the conditions in the if are:
            // 1) it's in front of camera plane so you don't see things behind you
            // 2) it's on the screen (left)
            // 3) it's on the screen (right)
            // 4) ZBuffer, with perpendicular distance
            if(transformY > 0 && stripe > 0 && stripe < OLED_WIDTH && transformY < rayResult[stripe].perpWallDist)
            {
                // for every pixel of the current stripe
                for(int32_t y = drawStartY; y < drawEndY; y++)
                {
                    // 256 and 128 factors to avoid floats
                    int32_t d = (y) * 256 - OLED_HEIGHT * 128 + spriteHeight * 128;
                    int32_t texY = ((d * texHeight) / spriteHeight) / 256;
                    // get current color from the texture
                    drawPixel(stripe, y, enemySpriteTex[(texX * texHeight) + texY]);
                }
            }
        }
    }
}

/**
 * Bubble sort algorithm to which sorts both order and dist by the values in dist
 *
 * @param order  Sprite indices to be sorted by dist
 * @param dist   The distances from the camera to the sprites
 * @param amount The number of values to sort
 */
void ICACHE_FLASH_ATTR sortSprites(int32_t* order, float* dist, int32_t amount)
{
    for (int32_t i = 0; i < amount - 1; i++)
    {
        // Last i elements are already in place
        for (int32_t j = 0; j < amount - i - 1; j++)
        {
            if (dist[j] < dist[j + 1])
            {
                float tmp = dist[j];
                dist[j] = dist[j + 1];
                dist[j + 1] = tmp;

                int32_t tmp2 = order[j];
                order[j] = order[j + 1];
                order[j + 1] = tmp2;
            }
        }
    }
}

/**
 * Update the camera position based on the current button state
 */
void ICACHE_FLASH_ATTR handleRayInput(void)
{
    static uint32_t time = 0; // time of current frame
    static uint32_t oldTime = 0; // time of previous frame

    // timing for input and FPS counter
    oldTime = time;
    time = system_get_time();
    // frameTime is the time this frame has taken, in seconds
    float frameTime = (time - oldTime) / 1000000.0;

    // speed modifiers
    float moveSpeed = frameTime * 5.0; // the constant value is in squares/second
    float rotSpeed = frameTime * 3.0; // the constant value is in radians/second

    // move forward if no wall in front of you
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

    // move backwards if no wall behind you
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

    // rotate to the right
    if(rButtonState & 0x04)
    {
        // both camera direction and camera plane must be rotated
        float oldDirX = dirX;
        dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
        dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
        float oldPlaneX = planeX;
        planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
        planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
    }

    // rotate to the left
    if(rButtonState & 0x01)
    {
        // both camera direction and camera plane must be rotated
        float oldDirX = dirX;
        dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
        dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
        float oldPlaneX = planeX;
        planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
        planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
    }
}
