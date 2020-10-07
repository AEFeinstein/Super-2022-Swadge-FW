/*==============================================================================
 * Includes
 *============================================================================*/

#include <math.h>
#include <stdlib.h>
#include <osapi.h>
#include <user_interface.h>
#include <mem.h>
#include "user_main.h"
#include "mode_raycaster.h"

#include "oled.h"
#include "assets.h"
#include "font.h"
#include "cndraw.h"

#include "buttons.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define mapWidth  24
#define mapHeight 24

#define texWidth  48
#define texHeight 48

#define NUM_SPRITES 20

#define ENEMY_SHOT_COOLDOWN  3000000

#define SHOT_ANIM_TIME       1000000

#define LONG_WALK_ANIM_TIME  3000000
#define WALK_ANIM_TIME       1000000
#define STEP_ANIM_TIME        250000

#define PLAYER_SHOT_COOLDOWN  300000

typedef enum
{
    E_IDLE,
    E_PICK_DIR_PLAYER,
    E_PICK_DIR_RAND,
    E_WALKING,
    E_SHOOTING,
    E_DYING
} enemyState_t;

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    uint8_t mapX;
    uint8_t mapY;
    uint8_t side;
    int32_t drawStart;
    int32_t drawEnd;
    float perpWallDist;
    float rayDirX;
    float rayDirY;
} rayResult_t;

typedef struct
{
    float posX;
    float posY;
    float dirX;
    float dirY;
    color* texture;
    int32_t texTimer;
    enemyState_t state;
    int32_t stateTimer;
    int32_t shotCooldown;
    bool isBackwards;
} raySprite_t;

typedef struct
{
    uint8_t rButtonState;
    float posX;
    float posY;
    float dirX;
    float dirY;
    float planeX;
    float planeY;
    int32_t shotCooldown;
    bool checkShot;

    // The enemies
    raySprite_t sprites[NUM_SPRITES];

    // arrays used to sort the sprites
    int32_t spriteOrder[NUM_SPRITES];
    float spriteDistance[NUM_SPRITES];

    // Storage for textures
    color stoneTex[texWidth * texHeight];
    color stripeTex[texWidth * texHeight];

    color w1[texWidth * texHeight];
    color w2[texWidth * texHeight];

    color s1[texWidth * texHeight];
    color s2[texWidth * texHeight];

    color d1[texWidth * texHeight];
    color d2[texWidth * texHeight];
    color d3[texWidth * texHeight];

    // Storage for HUD images
    pngHandle heart;
    pngHandle mnote;
    pngSequenceHandle gtr;
} raycaster_t;

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
void ICACHE_FLASH_ATTR handleRayInput(uint32_t tElapsed);
void ICACHE_FLASH_ATTR moveEnemies(uint32_t tElapsed);
float ICACHE_FLASH_ATTR Q_rsqrt( float number );
bool ICACHE_FLASH_ATTR checkLineToPlayer(raySprite_t* sprite, float pX, float pY);
void ICACHE_FLASH_ATTR setSpriteState(raySprite_t* sprite, enemyState_t state);
void ICACHE_FLASH_ATTR drawHUD(void);

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

raycaster_t* rc;

static const int32_t worldMap[mapWidth][mapHeight] =
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

// static const raySprite_t sprite[NUM_SPRITES] =
// {
//     {20.5, 11.5, 10},
//     {18.5, 4.5, 10},
//     {10.0, 4.5, 10},
//     {10.0, 12.5, 10},
//     {3.5, 6.5, 10},
//     {3.5, 20.5, 10},
//     {3.5, 14.5, 10},
//     {14.5, 20.5, 10},
//     {18.5, 10.5, 9},
//     {18.5, 11.5, 9},
//     {18.5, 12.5, 9},
//     {21.5, 1.5, 8},
//     {15.5, 1.5, 8},
//     {16.0, 1.8, 8},
//     {16.2, 1.2, 8},
//     {3.5,  2.5, 8},
//     {9.5, 15.5, 8},
//     {10.0, 15.1, 8},
//     {10.5, 15.8, 8},
// };

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

    rc = os_malloc(sizeof(raycaster_t));

    rc->rButtonState = 0;
    // x and y start position
    rc->posX = 22;
    rc->posY = 12;
    // initial direction vector
    rc->dirX = -1;
    rc->dirY = 0;
    // the 2d raycaster version of camera plane
    rc->planeX = 0;
    rc->planeY = 0.66;

    for(uint8_t i = 0; i < NUM_SPRITES; i++)
    {
        rc->sprites[i].posX = -1;
        rc->sprites[i].posY = -1;
    }

    int8_t spritesPlaced = 0;
    for(uint8_t x = 0; x < mapWidth; x++)
    {
        for(uint8_t y = 0; y < mapHeight; y++)
        {
            if(spritesPlaced < NUM_SPRITES && worldMap[x][y] == 0)
            {
                rc->sprites[spritesPlaced].posX = x;
                rc->sprites[spritesPlaced].posY = y;
                rc->sprites[spritesPlaced].dirX = 0;
                rc->sprites[spritesPlaced].dirX = 0;
                rc->sprites[spritesPlaced].isBackwards = false;
                setSpriteState(&(rc->sprites[spritesPlaced]), E_IDLE);
                spritesPlaced++;
            }
        }
    }

    pngHandle tmpPngHandle;

    // Load the enemy texture to RAM
    allocPngAsset("w1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->w1);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("w2.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->w2);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("s1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->s1);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("s2.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->s2);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("d1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->d1);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("d2.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->d2);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("d3.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->d3);
    freePngAsset(&tmpPngHandle);

    // Load the wall textures to RAM
    allocPngAsset("txstone.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->stoneTex);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("txstripe.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->stripeTex);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("heart.png", &(rc->heart));
    allocPngAsset("mnote.png", &(rc->mnote));
    allocPngSequence(&(rc->gtr), 5,
                     "gtr1.png",
                     "gtr2.png",
                     "gtr3.png",
                     "gtr4.png",
                     "gtr5.png");

    // TODO add a top level menu, difficulty, high scores
}

/**
 * Free all resources allocated in raycasterEnterMode
 */
void ICACHE_FLASH_ATTR raycasterExitMode(void)
{
    freePngAsset(&(rc->heart));
    freePngAsset(&(rc->mnote));
    freePngSequence(&(rc->gtr));
    os_free(rc);
    rc = NULL;
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
    rc->rButtonState = state;
}

/**
 * This function renders the scene and handles input. It is called as fast as
 * possible by user_main.c's procTask.
 */
void ICACHE_FLASH_ATTR raycasterProcess(void)
{
    static uint32_t tLastUs = 0; // time of current frame
    if(tLastUs == 0)
    {
        tLastUs = system_get_time();
    }
    else
    {
        uint32_t tNowUs = system_get_time();
        uint32_t tElapsedUs = tNowUs - tLastUs;
        tLastUs = tNowUs;

        handleRayInput(tElapsedUs);
        moveEnemies(tElapsedUs);
    }

    rayResult_t rayResult[OLED_WIDTH] = {{0}};

    clearDisplay();
    castRays(rayResult);
    drawTextures(rayResult);
    drawOutlines(rayResult);
    drawSprites(rayResult);
    drawHUD();
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
        float rayDirX = rc->dirX + rc->planeX * cameraX;
        float rayDirY = rc->dirY + rc->planeY * cameraX;

        // which box of the map we're in
        int32_t mapX = (int32_t)(rc->posX);
        int32_t mapY = (int32_t)(rc->posY);

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
            sideDistX = (rc->posX - mapX) * deltaDistX;
        }
        else
        {
            stepX = 1;
            sideDistX = (mapX + 1.0 - rc->posX) * deltaDistX;
        }

        if(rayDirY < 0)
        {
            stepY = -1;
            sideDistY = (rc->posY - mapY) * deltaDistY;
        }
        else
        {
            stepY = 1;
            sideDistY = (mapY + 1.0 - rc->posY) * deltaDistY;
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
            perpWallDist = (mapX - rc->posX + (1 - stepX) / 2) / rayDirX;
        }
        else
        {
            perpWallDist = (mapY - rc->posY + (1 - stepY) / 2) / rayDirY;
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
        else if(drawStart > OLED_HEIGHT)
        {
            drawStart = OLED_HEIGHT;
        }

        if(drawEnd < 0)
        {
            drawEnd = 0;
        }
        else if(drawEnd > OLED_HEIGHT)
        {
            drawEnd = OLED_HEIGHT;
        }

        // Pick a texture
        int32_t texNum = (worldMap[mapX][mapY] - 1) % 2;

        // calculate value of wallX, where exactly the wall was hit
        float wallX;
        if(side == 0)
        {
            wallX = rc->posY + rayResult[x].perpWallDist * rayResult[x].rayDirY;
        }
        else
        {
            wallX = rc->posX + rayResult[x].perpWallDist * rayResult[x].rayDirX;
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
            if(texNum)
            {
                drawPixelUnsafeC(x, y, rc->stoneTex[(texX * texHeight) + texY ]);
            }
            else
            {
                drawPixelUnsafeC(x, y, rc->stripeTex[(texX * texHeight) + texY ]);
            }
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
            int32_t ds = rayResult[x].drawStart;
            int32_t de = rayResult[x].drawEnd;
            if(ds < 0 || de > OLED_HEIGHT - 1)
            {
                ds = 0;
                de = OLED_HEIGHT - 1;
            }
            for(int32_t y = ds; y <= de; y++)
            {
                drawPixelUnsafeC(x, y, WHITE);
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
                if(rayResult[x].drawStart >= 0)
                {
                    drawPixelUnsafeC(x, rayResult[x].drawStart, WHITE);
                }
                if(rayResult[x].drawEnd < OLED_HEIGHT)
                {
                    drawPixelUnsafeC(x, rayResult[x].drawEnd, WHITE);
                }
            }
            else if((rayResult[x].drawEnd - rayResult[x].drawStart) >
                    (rayResult[x + 1].drawEnd - rayResult[x + 1].drawStart))
            {
                // This is a corner or edge, and this vertical strip is larger than the next one
                // Draw a vertical strip
                int32_t ds = rayResult[x].drawStart;
                int32_t de = rayResult[x].drawEnd;
                if(ds < 0 || de > OLED_HEIGHT - 1)
                {
                    ds = 0;
                    de = OLED_HEIGHT - 1;
                }
                for(int32_t y = ds; y <= de; y++)
                {
                    drawPixelUnsafeC(x, y, WHITE);
                }
            }
            else
            {
                // This is a corner or edge, and this vertical strip is smaller than the next one
                // Just draw top and bottom pixels, but make sure to draw a vertical line next
                if(rayResult[x].drawStart >= 0)
                {
                    drawPixelUnsafeC(x, rayResult[x].drawStart, WHITE);
                }
                if(rayResult[x].drawEnd < OLED_HEIGHT)
                {
                    drawPixelUnsafeC(x, rayResult[x].drawEnd, WHITE);
                }
                // make sure to draw a vertical line next
                drawVertNext = true;
            }
        }
        else
        {
            // These are the very last pixels, nothing to compare to
            if(rayResult[x].drawStart >= 0)
            {
                drawPixelUnsafeC(x, rayResult[x].drawStart, WHITE);
            }
            if(rayResult[x].drawEnd < OLED_HEIGHT)
            {
                drawPixelUnsafeC(x, rayResult[x].drawEnd, WHITE);
            }
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
    // Track if any sprite was shot
    int16_t spriteIdxShot = -1;

    // sort sprites from far to close
    for(uint32_t i = 0; i < NUM_SPRITES; i++)
    {
        rc->spriteOrder[i] = i;
        // sqrt not taken, unneeded
        rc->spriteDistance[i] = ((rc->posX - rc->sprites[i].posX) * (rc->posX - rc->sprites[i].posX) +
                                 (rc->posY - rc->sprites[i].posY) * (rc->posY - rc->sprites[i].posY));
    }
    sortSprites(rc->spriteOrder, rc->spriteDistance, NUM_SPRITES);

    // after sorting the sprites, do the projection and draw them
    for(uint32_t i = 0; i < NUM_SPRITES; i++)
    {
        // Skip over the sprite if posX is negative
        if(rc->sprites[rc->spriteOrder[i]].posX < 0)
        {
            continue;
        }
        // translate sprite position to relative to camera
        float spriteX = rc->sprites[rc->spriteOrder[i]].posX - rc->posX;
        float spriteY = rc->sprites[rc->spriteOrder[i]].posY - rc->posY;

        // transform sprite with the inverse camera matrix
        // [ planeX dirX ] -1                                  [ dirY     -dirX ]
        // [             ]    =  1/(planeX*dirY-dirX*planeY) * [                ]
        // [ planeY dirY ]                                     [ -planeY planeX ]

        // required for correct matrix multiplication
        float invDet = 1.0 / (rc->planeX * rc->dirY - rc->dirX * rc->planeY);

        float transformX = invDet * (rc->dirY * spriteX - rc->dirX * spriteY);
        // this is actually the depth inside the screen, that what Z is in 3D
        float transformY = invDet * (-rc->planeY * spriteX + rc->planeX * spriteY);

        // If this is negative, the texture isn't going to be drawn, so just stop here
        if(transformY < 0)
        {
            continue;
        }

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
        if(drawEndY > OLED_HEIGHT)
        {
            drawEndY = OLED_HEIGHT;
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
        if(drawEndX > OLED_WIDTH)
        {
            drawEndX = OLED_WIDTH;
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
            if(transformY < rayResult[stripe].perpWallDist)
            {
                // for every pixel of the current stripe
                for(int32_t y = drawStartY; y < drawEndY; y++)
                {
                    // 256 and 128 factors to avoid floats
                    int32_t d = (y) * 256 - OLED_HEIGHT * 128 + spriteHeight * 128;
                    int32_t texY = ((d * texHeight) / spriteHeight) / 256;
                    // get current color from the texture
                    drawPixelUnsafeC(stripe, y, rc->sprites[rc->spriteOrder[i]].texture[(texX * texHeight) + texY]);

                    if(true == rc->checkShot && (stripe == 63 || stripe == 64))
                    {
                        spriteIdxShot = rc->spriteOrder[i];
                    }
                }
            }
        }
    }

    if(spriteIdxShot >= 0)
    {
        float distSqr = ((rc->sprites[spriteIdxShot].posX - rc->posX) * (rc->sprites[spriteIdxShot].posX - rc->posX)) +
                        ((rc->sprites[spriteIdxShot].posY - rc->posY) * (rc->sprites[spriteIdxShot].posY - rc->posY));
        if(distSqr < 36.0f)
        {
            // TODO track enemy health
            os_printf("You shot %d! (%d)\n", spriteIdxShot, (int)distSqr);
        }
    }
    rc->checkShot = false;
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
void ICACHE_FLASH_ATTR handleRayInput(uint32_t tElapsedUs)
{
    float frameTime = (tElapsedUs) / 1000000.0;

    // speed modifiers
    float moveSpeed = frameTime * 5.0; // the constant value is in squares/second
    float rotSpeed = frameTime * 3.0; // the constant value is in radians/second

    // move forward if no wall in front of you
    if(rc->rButtonState & 0x08)
    {
        if(worldMap[(int32_t)(rc->posX + rc->dirX * moveSpeed)][(int32_t)(rc->posY)] == false)
        {
            rc->posX += rc->dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(rc->posX)][(int32_t)(rc->posY + rc->dirY * moveSpeed)] == false)
        {
            rc->posY += rc->dirY * moveSpeed;
        }
    }

    // move backwards if no wall behind you
    if(rc->rButtonState & 0x02)
    {
        if(worldMap[(int32_t)(rc->posX - rc->dirX * moveSpeed)][(int32_t)(rc->posY)] == false)
        {
            rc->posX -= rc->dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(rc->posX)][(int32_t)(rc->posY - rc->dirY * moveSpeed)] == false)
        {
            rc->posY -= rc->dirY * moveSpeed;
        }
    }

    // rotate to the right
    if(rc->rButtonState & 0x04)
    {
        // both camera direction and camera plane must be rotated
        float oldDirX = rc->dirX;
        rc->dirX = rc->dirX * cos(-rotSpeed) - rc->dirY * sin(-rotSpeed);
        rc->dirY = oldDirX * sin(-rotSpeed) + rc->dirY * cos(-rotSpeed);
        float oldPlaneX = rc->planeX;
        rc->planeX = rc->planeX * cos(-rotSpeed) - rc->planeY * sin(-rotSpeed);
        rc->planeY = oldPlaneX * sin(-rotSpeed) + rc->planeY * cos(-rotSpeed);
    }

    // rotate to the left
    if(rc->rButtonState & 0x01)
    {
        // both camera direction and camera plane must be rotated
        float oldDirX = rc->dirX;
        rc->dirX = rc->dirX * cos(rotSpeed) - rc->dirY * sin(rotSpeed);
        rc->dirY = oldDirX * sin(rotSpeed) + rc->dirY * cos(rotSpeed);
        float oldPlaneX = rc->planeX;
        rc->planeX = rc->planeX * cos(rotSpeed) - rc->planeY * sin(rotSpeed);
        rc->planeY = oldPlaneX * sin(rotSpeed) + rc->planeY * cos(rotSpeed);
    }

    if(rc->shotCooldown > 0)
    {
        rc->shotCooldown -= tElapsedUs;
    }
    else
    {
        rc->shotCooldown = 0;
    }

    if(rc->rButtonState & 0x10 && 0 == rc->shotCooldown)
    {
        rc->shotCooldown = PLAYER_SHOT_COOLDOWN;
        rc->checkShot = true;
    }
}

/**
 * Fast inverse square root
 * See: https://en.wikipedia.org/wiki/Fast_inverse_square_root
 *
 * @param number
 * @return float
 */
float ICACHE_FLASH_ATTR Q_rsqrt( float number )
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    i  = * ( long* ) &y;          // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 ); // what the fuck?
    y  = * ( float* ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    //  y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}

/**
 * @brief TODO
 *
 * Move sprites around
 *
 * @param tElapsed
 */
void ICACHE_FLASH_ATTR moveEnemies(uint32_t tElapsedUs)
{
    float frameTime = (tElapsedUs) / 1000000.0;

    for(uint8_t i = 0; i < NUM_SPRITES; i++)
    {
        // Skip over sprites with negative position
        if(rc->sprites[i].posX < 0)
        {
            continue;
        }

        // Always run down the shot cooldown, regardless of state
        if(rc->sprites[i].shotCooldown > 0)
        {
            rc->sprites[i].shotCooldown -= tElapsedUs;
        }

        switch (rc->sprites[i].state)
        {
            default:
            case E_IDLE:
            {
                // Check if player is close to move to E_PICK_DIR_PLAYER
                float toPlayerX = rc->posX - rc->sprites[i].posX;
                float toPlayerY = rc->posY - rc->sprites[i].posY;
                float magSqr = (toPlayerX * toPlayerX) + (toPlayerY * toPlayerY);

                // Less than 8 units away (avoid the sqrt!)
                if(magSqr < 64)
                {
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                }
                break;
            }
            case E_PICK_DIR_RAND:
            {
                // Get a random unit vector in the first quadrant
                rc->sprites[i].dirX = (os_random() % 90) / 90.0f;
                rc->sprites[i].dirY = 1 / Q_rsqrt(1 - (rc->sprites[i].dirX * rc->sprites[i].dirX));

                // Randomize the unit vector quadrant
                switch(os_random() % 4)
                {
                    default:
                    case 0:
                    {
                        break;
                    }
                    case 1:
                    {
                        rc->sprites[i].dirX = -rc->sprites[i].dirX;
                        break;
                    }
                    case 2:
                    {
                        rc->sprites[i].dirY = -rc->sprites[i].dirY;
                        break;
                    }
                    case 3:
                    {
                        rc->sprites[i].dirX = -rc->sprites[i].dirX;
                        rc->sprites[i].dirY = -rc->sprites[i].dirY;
                        break;
                    }
                }

                rc->sprites[i].isBackwards = false;

                // And let the sprite walk for a bit
                setSpriteState(&(rc->sprites[i]), E_WALKING);
                // Walk for a little extra in the random direction
                rc->sprites[i].stateTimer = LONG_WALK_ANIM_TIME;
                break;
            }
            case E_PICK_DIR_PLAYER:
            {
                // Find the vector from the enemy to the player
                float toPlayerX = rc->posX - rc->sprites[i].posX;
                float toPlayerY = rc->posY - rc->sprites[i].posY;
                float magSqr = (toPlayerX * toPlayerX) + (toPlayerY * toPlayerY);

                // Randomly take a shot
                if(rc->sprites[i].shotCooldown <= 0 &&           // If the sprite is not in cooldown
                        (uint32_t)magSqr < 64 &&                 // If the player is no more than 8 units away
                        ((os_random() % 64) > (uint32_t)magSqr)) // And distance-weighted RNG says so
                {
                    // Take the shot!
                    setSpriteState(&(rc->sprites[i]), E_SHOOTING);
                }
                else // Pick a direction to walk in
                {
                    // Normalize the vector
                    float invSqr = Q_rsqrt(magSqr);
                    toPlayerX *= invSqr;
                    toPlayerY *= invSqr;

                    // Rotate the vector, maybe
                    switch(os_random() % 3)
                    {
                        default:
                        case 0:
                        {
                            // Straight ahead!
                            break;
                        }
                        case 1:
                        {
                            // Rotate 45 degrees
                            toPlayerX = toPlayerX * 0.70710678f - toPlayerY * 0.70710678f;
                            toPlayerY = toPlayerY * 0.70710678f + toPlayerX * 0.70710678f;
                            break;
                        }
                        case 2:
                        {
                            // Rotate -45 degrees
                            toPlayerX = toPlayerX * 0.70710678f + toPlayerY * 0.70710678f;
                            toPlayerY = toPlayerY * 0.70710678f - toPlayerX * 0.70710678f;
                            break;
                        }
                    }

                    // Set the sprite's direction
                    rc->sprites[i].dirX = toPlayerX;
                    rc->sprites[i].dirY = toPlayerY;
                    rc->sprites[i].isBackwards = false;

                    // And let the sprite walk for a bit
                    setSpriteState(&(rc->sprites[i]), E_WALKING);
                }
                break;
            }
            case E_WALKING:
            {
                // Find the vector from the enemy to the player
                float toPlayerX = rc->posX - rc->sprites[i].posX;
                float toPlayerY = rc->posY - rc->sprites[i].posY;
                float magSqr = (toPlayerX * toPlayerX) + (toPlayerY * toPlayerY);

                // See if we're done walking
                rc->sprites[i].stateTimer -= tElapsedUs;
                if (rc->sprites[i].stateTimer <= 0)
                {
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                    break;
                }

                // Flip sprites for a walking animation
                rc->sprites[i].texTimer -= tElapsedUs;
                while(rc->sprites[i].texTimer <= 0)
                {
                    rc->sprites[i].texTimer += STEP_ANIM_TIME;
                    if(rc->sprites[i].texture == rc->w1)
                    {
                        rc->sprites[i].texture = rc->w2;
                    }
                    else
                    {
                        rc->sprites[i].texture = rc->w1;
                    }
                }

                // Find the new position
                float moveSpeed = frameTime * 1.0; // the constant value is in squares/second

                if(magSqr < 0.5f && false == rc->sprites[i].isBackwards)
                {
                    rc->sprites[i].dirX = -rc->sprites[i].dirX;
                    rc->sprites[i].dirY = -rc->sprites[i].dirY;
                    rc->sprites[i].isBackwards = true;
                }
                else if(magSqr > 1.5f && true == rc->sprites[i].isBackwards)
                {
                    rc->sprites[i].dirX = -rc->sprites[i].dirX;
                    rc->sprites[i].dirY = -rc->sprites[i].dirY;
                    rc->sprites[i].isBackwards = false;
                }

                // Move backwards if too close
                float newPosX = rc->sprites[i].posX + (rc->sprites[i].dirX * moveSpeed);
                float newPosY = rc->sprites[i].posY + (rc->sprites[i].dirY * moveSpeed);

                // Integer-ify it
                int newPosXi = newPosX;
                int newPosYi = newPosY;

                // Make sure the new position is in bounds
                bool moveIsValid = false;
                if(     (0 <= newPosXi && newPosXi < mapWidth) &&
                        (0 <= newPosYi && newPosYi < mapHeight) &&
                        // And that it's not occupied by a wall
                        (worldMap[newPosXi][newPosYi] == 0))
                {
                    // Make sure the new square is unoccupied
                    bool newSquareOccupied = false;
                    for(uint8_t oth = 0; oth < NUM_SPRITES; oth++)
                    {
                        if(oth != i && rc->sprites[oth].posX > 0)
                        {
                            if(     (int)rc->sprites[oth].posX == newPosXi &&
                                    (int)rc->sprites[oth].posY == newPosYi )
                            {
                                newSquareOccupied = true;
                                break;
                            }
                        }
                    }
                    if(false == newSquareOccupied)
                    {
                        moveIsValid = true;
                    }
                }

                // If the move is valid, move there
                if(moveIsValid)
                {
                    rc->sprites[i].posX = newPosX;
                    rc->sprites[i].posY = newPosY;
                }
                else
                {
                    // Else pick a new direction, totally random
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_RAND);
                }
                break;
            }
            case E_SHOOTING:
            {
                // Animate a shot
                rc->sprites[i].texTimer -= tElapsedUs;
                if(rc->sprites[i].texTimer <= 0)
                {
                    // After the shot, go to E_PICK_DIR_PLAYER
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                }
                else if(rc->sprites[i].texTimer < SHOT_ANIM_TIME / 2 &&
                        rc->sprites[i].shotCooldown <= 0)
                {
                    rc->sprites[i].shotCooldown = ENEMY_SHOT_COOLDOWN;
                    // After 0.5s switch to next texture
                    rc->sprites[i].texture = rc->s2;
                    // Check if the sprite can still see the player
                    if(checkLineToPlayer(&rc->sprites[i], rc->posX, rc->posY))
                    {
                        // TODO track player health (maybe RNG damage)
                    }
                }
                break;
            }
            case E_DYING:
            {
                // TODO animate death
                break;
            }
        }
    }
}

/**
 * Use DDA to draw a line between the player and a sprite, and check if there
 * are any walls between the two. This line drawing algorithm is the same one
 * used to cast rays from the player to walls
 */
bool ICACHE_FLASH_ATTR checkLineToPlayer(raySprite_t* sprite, float pX, float pY)
{
    // If the sprite and the player are in the same cell
    if(((int32_t)sprite->posX == (int32_t)pX) &&
            ((int32_t)sprite->posY == (int32_t)pY))
    {
        // We can definitely draw a line between the two
        return true;
    }

    // calculate ray position and direction
    // x-coordinate in camera space
    float rayDirX = sprite->posX - pX;
    float rayDirY = sprite->posY - pY;

    // which box of the map we're in
    int32_t mapX = (int32_t)(pX);
    int32_t mapY = (int32_t)(pY);

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

    // calculate step and initial sideDist
    if(rayDirX < 0)
    {
        stepX = -1;
        sideDistX = (pX - mapX) * deltaDistX;
    }
    else
    {
        stepX = 1;
        sideDistX = (mapX + 1.0 - pX) * deltaDistX;
    }

    if(rayDirY < 0)
    {
        stepY = -1;
        sideDistY = (pY - mapY) * deltaDistY;
    }
    else
    {
        stepY = 1;
        sideDistY = (mapY + 1.0 - pY) * deltaDistY;
    }

    // perform DDA until a wall is hit or the ray reaches the sprite
    while (true)
    {
        // jump to next map square, OR in x-direction, OR in y-direction
        if(sideDistX < sideDistY)
        {
            sideDistX += deltaDistX;
            mapX += stepX;
        }
        else
        {
            sideDistY += deltaDistY;
            mapY += stepY;
        }

        // Check if ray has hit a wall
        if(worldMap[mapX][mapY] > 0)
        {
            // There is a wall between the player and the sprite
            return false;
        }
        else if(mapX == (int32_t)sprite->posX && mapY == (int32_t)sprite->posY)
        {
            // Ray reaches from the player to the sprite unobstructed
            return true;
        }
    }
    // Shouldn't reach here
    return false;
}

/**
 * Set the sprite state and associated timers and textures
 *
 * @param sprite The sprite to set state for
 * @param state  The state to set
 */
void ICACHE_FLASH_ATTR setSpriteState(raySprite_t* sprite, enemyState_t state)
{
    // Set the state
    sprite->state = state;

    // Set timers and textures
    switch(state)
    {
        default:
        case E_IDLE:
        {
            sprite->stateTimer = 0;
            sprite->texture = rc->w1;
            sprite->texTimer = 0;
            break;
        }
        case E_PICK_DIR_PLAYER:
        case E_PICK_DIR_RAND:
        {
            sprite->stateTimer = 0;
            sprite->texture = rc->w1;
            sprite->texTimer = 0;
            break;
        }
        case E_WALKING:
        {
            sprite->stateTimer = WALK_ANIM_TIME;
            sprite->texture = rc->w1;
            sprite->texTimer = STEP_ANIM_TIME;
            break;
        }
        case E_SHOOTING:
        {
            sprite->stateTimer = SHOT_ANIM_TIME;
            sprite->texture = rc->s1;
            sprite->texTimer = SHOT_ANIM_TIME;
            break;
        }
        case E_DYING:
        {
            // TODO Set timers for death animation
            sprite->stateTimer = 0;
            sprite->texture = rc->d1;
            sprite->texTimer = 0;
            break;
        }
    }
}

/**
 * Draw the HUD, including the weapon and health
 */
void ICACHE_FLASH_ATTR drawHUD(void)
{
    // Figure out widths for note display
    char notes[8] = {0};
    ets_snprintf(notes, sizeof(notes) - 1, "%d", 88);
    int16_t noteWidth = textWidth(notes, IBM_VGA_8);

    // Clear area behind note display
    fillDisplayArea(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1,
                    noteWidth + rc->mnote.width + 1, OLED_HEIGHT,
                    BLACK);

    // Draw note display
    drawPng(&(rc->mnote),
            0,
            OLED_HEIGHT - rc->mnote.height,
            false, false, 0);
    plotText(rc->mnote.width + 2,
             OLED_HEIGHT - FONT_HEIGHT_IBMVGA8,
             notes, IBM_VGA_8, WHITE);

    // Figure out widths for health display
    char health[8] = {0};
    ets_snprintf(health, sizeof(health) - 1, "%d", 99);
    int16_t healthWidth = textWidth(health, IBM_VGA_8);
    int16_t healthDrawX = OLED_WIDTH - rc->heart.width - 1 - healthWidth;

    // Clear area behind health display
    fillDisplayArea(healthDrawX - 1, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1,
                    OLED_WIDTH, OLED_HEIGHT,
                    BLACK);

    // Draw health display
    drawPng(&(rc->heart),
            OLED_WIDTH - rc->heart.width,
            OLED_HEIGHT - rc->heart.height,
            false, false, 0);
    plotText(healthDrawX,
             OLED_HEIGHT - FONT_HEIGHT_IBMVGA8,
             health, IBM_VGA_8, WHITE);

    // Draw Guitar
    if(0 == rc->shotCooldown)
    {
        drawPngSequence(&(rc->gtr),
                        (OLED_WIDTH - rc->gtr.handles->width) / 2,
                        (OLED_HEIGHT - rc->gtr.handles->height),
                        false, false, 0, 0);
    }
    else
    {
        int8_t idx = (rc->gtr.count * (PLAYER_SHOT_COOLDOWN - rc->shotCooldown)) / PLAYER_SHOT_COOLDOWN;
        if(idx >= rc->gtr.count)
        {
            idx = rc->gtr.count - 1;
        }
        drawPngSequence(&(rc->gtr),
                        (OLED_WIDTH - rc->gtr.handles->width) / 2,
                        (OLED_HEIGHT - rc->gtr.handles->height),
                        false, false, 0, idx);
    }
}
