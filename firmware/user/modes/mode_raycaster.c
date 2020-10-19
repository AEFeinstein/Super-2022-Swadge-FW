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

#include "menu2d.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define mapWidth  48
#define mapHeight 48

#define texWidth  48
#define texHeight 48

#define NUM_SPRITES 57

#define ENEMY_SHOT_COOLDOWN  3000000

#define SHOOTING_ANIM_TIME   1000000
#define GOT_SHOT_ANIM_TIME   1000000

#define LONG_WALK_ANIM_TIME  3000000
#define WALK_ANIM_TIME       1000000
#define STEP_ANIM_TIME        250000

#define PLAYER_SHOT_COOLDOWN  300000

#define LED_ON_TIME           500000

#define ENEMY_HEALTH   2
#define PLAYER_HEALTH 99

#define NUM_IMP_WALK_FRAMES 2

typedef enum
{
    E_IDLE,
    E_PICK_DIR_PLAYER,
    E_PICK_DIR_RAND,
    E_WALKING,
    E_SHOOTING,
    E_GOT_SHOT,
    E_DEAD
} enemyState_t;

typedef enum
{
    RC_MENU,
    RC_GAME,
    RC_GAME_OVER,
    RC_SCORES
} raycasterMode_t;

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
    // Sprite location and direction
    float posX;
    float posY;
    float dirX;
    float dirY;

    // Sprite texture
    color* texture;
    int32_t texTimer;
    int8_t texFrame;
    bool mirror;

    // Sprite logic
    enemyState_t state;
    int32_t stateTimer;
    int32_t shotCooldown;
    bool isBackwards;
    int32_t health;
} raySprite_t;

typedef struct
{
    raycasterMode_t mode;
    menu_t* menu;

    uint8_t rButtonState;
    float posX;
    float posY;
    float dirX;
    float dirY;
    float planeX;
    float planeY;
    int32_t shotCooldown;
    bool checkShot;
    int32_t health;
    uint32_t tRoundStartedUs;
    uint32_t tRoundElapsed;

    // The enemies
    raySprite_t sprites[NUM_SPRITES];
    uint8_t liveSprites;
    uint8_t kills;

    // arrays used to sort the sprites
    int32_t spriteOrder[NUM_SPRITES];
    float spriteDistance[NUM_SPRITES];

    // Storage for textures
    color stoneTex[texWidth * texHeight];
    color stripeTex[texWidth * texHeight];
    color brickTex[texWidth * texHeight];
    color sinTex[texWidth * texHeight];

    color impWalk[NUM_IMP_WALK_FRAMES][texWidth * texHeight];

    color s1[texWidth * texHeight];
    color s2[texWidth * texHeight];

    color d1[texWidth * texHeight];
    color d2[texWidth * texHeight];
    color d3[texWidth * texHeight];

    // Storage for HUD images
    pngHandle heart;
    pngHandle mnote;
    pngSequenceHandle gtr;

    // For LEDs
    timer_t ledTimer;
    uint32_t closestDist;
    int32_t gotShotTimer;
    int32_t shotSomethingTimer;
} raycaster_t;

typedef enum
{
    WMT_W1 = 0,
    WMT_W2 = 1,
    WMT_W3 = 2,
    WMT_C  = 3,
    WMT_E  = 4,
    WMT_S  = 5,
} WorldMapTile_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR raycasterEnterMode(void);
void ICACHE_FLASH_ATTR raycasterExitMode(void);
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state, int32_t button, int32_t down);
void ICACHE_FLASH_ATTR raycasterMenuButtonCallback(const char* selected);
bool ICACHE_FLASH_ATTR raycasterRenderTask(void);
void ICACHE_FLASH_ATTR raycasterGameRenderer(void);
void ICACHE_FLASH_ATTR raycasterLedTimer(void* arg);
void ICACHE_FLASH_ATTR raycasterDrawScores(void);
void ICACHE_FLASH_ATTR raycasterEndRound(void);
void ICACHE_FLASH_ATTR raycasterDrawRoundOver(void);

void ICACHE_FLASH_ATTR moveEnemies(uint32_t tElapsed);
void ICACHE_FLASH_ATTR handleRayInput(uint32_t tElapsed);

void ICACHE_FLASH_ATTR castRays(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawTextures(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawOutlines(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawSprites(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawHUD(void);

void ICACHE_FLASH_ATTR raycasterInitGame(const char* difficulty);
void ICACHE_FLASH_ATTR sortSprites(int32_t* order, float* dist, int32_t amount);
float ICACHE_FLASH_ATTR Q_rsqrt( float number );
bool ICACHE_FLASH_ATTR checkLineToPlayer(raySprite_t* sprite, float pX, float pY);
void ICACHE_FLASH_ATTR setSpriteState(raySprite_t* sprite, enemyState_t state);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode raycasterMode =
{
    .modeName = "raycaster",
    .fnEnterMode = raycasterEnterMode,
    .fnExitMode = raycasterExitMode,
    .fnRenderTask = raycasterRenderTask,
    .fnButtonCallback = raycasterButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "ray-menu.gif"
};

raycaster_t* rc;

static const WorldMapTile_t worldMap[mapWidth][mapHeight] =
{
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, },
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 5, 4, 4, 0, 0, 0, 4, 4, 5, 4, 4, 0, 4, 2, 4, 4, 5, 4, 5, 4, 3, 3, 3, 4, 4, 4, 4, 4, 4, 2, },
    {4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 0, 0, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 4, 2, },
    {1, 1, 1, 1, 4, 3, 5, 3, 4, 3, 4, 3, 4, 1, 1, 0, 0, 0, 0, 4, 4, 5, 5, 5, 4, 4, 0, 0, 0, 0, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {2, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 2, 4, 4, 4, 4, 4, 4, 3, 3, 3, 4, 5, 4, 5, 4, 4, 2, },
    {2, 4, 1, 1, 4, 3, 5, 3, 4, 3, 4, 3, 4, 1, 4, 0, 0, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 0, 4, 2, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 0, 4, 4, 5, 4, 4, 0, 0, 0, 4, 4, 5, 4, 4, 0, 4, 2, 4, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, },
    {2, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 4, 4, 4, 4, 0, 4, 2, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 4, 5, 4, 5, 4, 3, 3, 3, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {2, 4, 5, 4, 4, 4, 4, 2, 4, 4, 4, 4, 5, 4, 2, 0, 4, 5, 5, 4, 4, 4, 4, 4, 4, 5, 4, 4, 5, 4, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 4, 2, },
    {2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 4, 5, 5, 4, 4, 3, 3, 3, 4, 4, 3, 3, 4, 4, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {2, 4, 4, 4, 3, 4, 4, 5, 4, 4, 3, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 3, 4, 4, 4, 5, 4, 4, 5, 4, 0, 2, 4, 4, 4, 4, 4, 4, 3, 3, 3, 4, 5, 4, 4, 4, 4, 2, },
    {2, 4, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 4, 4, 4, 4, 3, 4, 3, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 2, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, },
    {2, 2, 2, 4, 5, 4, 4, 3, 4, 4, 5, 4, 2, 2, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 1, 1, 1, 1, 1, 1, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, },
    {2, 4, 4, 4, 4, 4, 3, 4, 3, 4, 4, 4, 4, 4, 2, 0, 4, 4, 4, 4, 5, 5, 4, 4, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {2, 4, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 5, 5, 4, 4, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {2, 4, 4, 4, 3, 4, 4, 5, 4, 4, 3, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 0, 1, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 1, 4, },
    {2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 3, 3, 4, 4, 4, 0, 1, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 1, 4, },
    {2, 4, 5, 4, 4, 4, 4, 2, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 4, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 4, 4, 1, 4, },
    {2, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 4, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 4, 4, 1, 4, },
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 3, 4, 4, 1, 4, },
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 5, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 2, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 2, 1, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 2, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 2, 1, 4, 4, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 4, 4, 2, 1, 4, 4, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 2, 1, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 2, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 2, 4, 4, 3, 3, 3, 4, 4, 5, 4, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 2, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 2, 4, 4, 4, 5, 4, 3, 3, 3, 4, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 1, 4, 4, 1, 4, },
    {1, 1, 1, 1, 4, 4, 1, 1, 1, 1, 1, 1, 4, 2, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 4, 4, 1, 4, },
    {0, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 4, },
    {0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 4, 4, 3, 3, 3, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 2, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 4, 5, 4, 0, 0, 0, 0, 0, 4, 4, 4, },
    {0, 4, 5, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 2, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 5, 3, 4, 5, 3, 4, 5, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 5, 4, 3, 3, 4, 4, 4, 0, 4, 0, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 2, 0, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 3, 3, 4, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 0, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 0, 4, 4, 3, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, },
};

static const char rc_title[]  = "RAYCAST FPS";
static const char rc_easy[]   = "EASY";
static const char rc_med[]    = "MEDIUM";
static const char rc_hard[]   = "HARD";
static const char rc_scores[] = "SCORES";
static const char rc_quit[]   = "QUIT";

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Set up the raycaster by creating some simple textures and loading other
 * textures from assets
 */
void ICACHE_FLASH_ATTR raycasterEnterMode(void)
{
    os_printf("malloc %d\n", sizeof(raycaster_t));
    os_printf("system_get_free_heap_size %d\n", system_get_free_heap_size());
    rc = os_malloc(sizeof(raycaster_t));
    ets_memset(rc, 0, sizeof(raycaster_t));

    rc->mode = RC_MENU;

    rc->menu = initMenu(rc_title, raycasterMenuButtonCallback);
    addRowToMenu(rc->menu);
    addItemToRow(rc->menu, rc_easy);
    addItemToRow(rc->menu, rc_med);
    addItemToRow(rc->menu, rc_hard);
    addRowToMenu(rc->menu);
    addItemToRow(rc->menu, rc_scores);
    addRowToMenu(rc->menu);
    addItemToRow(rc->menu, rc_quit);

    enableDebounce(false);

    pngHandle tmpPngHandle;

    // Load the enemy texture to RAM
    allocPngAsset("impw0.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->impWalk[0]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("impw1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->impWalk[1]);
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

    allocPngAsset("txbrick.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->brickTex);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("txsinw.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->sinTex);
    freePngAsset(&tmpPngHandle);

    // Load the HUD assets
    allocPngAsset("heart.png", &(rc->heart));
    allocPngAsset("mnote.png", &(rc->mnote));
    allocPngSequence(&(rc->gtr), 5,
                     "gtr1.png",
                     "gtr2.png",
                     "gtr3.png",
                     "gtr4.png",
                     "gtr5.png");

    rc->closestDist = 0xFFFFFFFF;
    timerSetFn(&(rc->ledTimer), raycasterLedTimer, NULL);
    timerArm(&(rc->ledTimer), 10, true);

    os_printf("system_get_free_heap_size %d\n", system_get_free_heap_size());
}

/**
 * Free all resources allocated in raycasterEnterMode
 */
void ICACHE_FLASH_ATTR raycasterExitMode(void)
{
    deinitMenu(rc->menu);
    freePngAsset(&(rc->heart));
    freePngAsset(&(rc->mnote));
    freePngSequence(&(rc->gtr));
    timerDisarm(&(rc->ledTimer));
    timerFlush();
    os_free(rc);
    rc = NULL;
}

/**
 * Initialize the game state
 */
void ICACHE_FLASH_ATTR raycasterInitGame(const char* difficulty)
{
    rc->rButtonState = 0;
    // x and y start position
    rc->posY = 10.5;
    rc->posX = 40;
    // initial direction vector
    rc->dirX = 1;
    rc->dirY = 0;
    // the 2d raycaster version of camera plane
    rc->planeX = 0;
    rc->planeY = -0.66;

    for(uint8_t i = 0; i < NUM_SPRITES; i++)
    {
        rc->sprites[i].posX = -1;
        rc->sprites[i].posY = -1;
    }

    // Set health and number of enemies based on the difficulty
    uint16_t diffMod = 0;
    if(rc_easy == difficulty)
    {
        rc->health = PLAYER_HEALTH;
        diffMod = 2;
    }
    else if(rc_med == difficulty)
    {
        rc->health = (2 * PLAYER_HEALTH) / 3;
        diffMod = 3;
    }
    else if(rc_hard == difficulty)
    {
        rc->health = PLAYER_HEALTH / 2;
        diffMod = 1;
    }

    rc->liveSprites = 0;
    uint16_t spawnIdx = 0;
#ifndef TEST_GAME_OVER
    for(uint8_t x = 0; x < mapWidth; x++)
    {
        for(uint8_t y = 0; y < mapHeight; y++)
        {
            if(worldMap[x][y] == WMT_S)
            {
                if(rc->liveSprites < NUM_SPRITES && (((spawnIdx % diffMod) > 0) || (rc_hard == difficulty)))
                {
                    rc->sprites[rc->liveSprites].posX = x;
                    rc->sprites[rc->liveSprites].posY = y;
                    rc->sprites[rc->liveSprites].dirX = 0;
                    rc->sprites[rc->liveSprites].dirX = 0;
                    rc->sprites[rc->liveSprites].shotCooldown = 0;
                    rc->sprites[rc->liveSprites].isBackwards = false;
                    rc->sprites[rc->liveSprites].health = ENEMY_HEALTH;
                    setSpriteState(&(rc->sprites[rc->liveSprites]), E_IDLE);
                    rc->liveSprites++;
                }
                spawnIdx++;
            }
        }
    }
#else
    rc->sprites[rc->liveSprites].posX = 45;
    rc->sprites[rc->liveSprites].posY = 2 ;
    rc->sprites[rc->liveSprites].dirX = 0;
    rc->sprites[rc->liveSprites].dirX = 0;
    rc->sprites[rc->liveSprites].shotCooldown = 0;
    rc->sprites[rc->liveSprites].isBackwards = false;
    rc->sprites[rc->liveSprites].health = ENEMY_HEALTH;
    setSpriteState(&(rc->sprites[rc->liveSprites]), E_IDLE);
    rc->liveSprites++;
#endif

    rc->tRoundStartedUs = system_get_time();
}

/**
 * Simple button callback which saves the state of all buttons
 *
 * @param state  A bitmask with all the current button states
 * @param button The button that caused this interrupt
 * @param down   true if the button was pushed, false if it was released
 */
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state, int32_t button, int32_t down)
{
    switch(rc->mode)
    {
        default:
        case RC_MENU:
        {
            if(down)
            {
                menuButton(rc->menu, button);
            }
            break;
        }
        case RC_GAME:
        {
            rc->rButtonState = state;
            break;
        }
        case RC_GAME_OVER:
        {
            if(down)
            {
                rc->mode = RC_MENU;
            }
            break;
        }
        case RC_SCORES:
        {
            if(down)
            {
                rc->mode = RC_MENU;
            }
            break;
        }
    }
}

/**
 * Button callback from the top level menu when an item is selected
 *
 * @param selected The string that was selected
 */
void ICACHE_FLASH_ATTR raycasterMenuButtonCallback(const char* selected)
{
    if(rc_quit == selected)
    {
        switchToSwadgeMode(0);
    }
    else if ((rc_easy == selected) || (rc_med == selected) || (rc_hard == selected))
    {
        // Start the game!
        raycasterInitGame(selected);
        rc->mode = RC_GAME;
    }
    else if (rc_scores == selected)
    {
        // Show some scores
        rc->mode = RC_SCORES;
    }
}

/**
 * This is called whenever there is a screen to render
 * Draw either the menu or the game
 *
 * @return true to always draw the screen
 */
bool ICACHE_FLASH_ATTR raycasterRenderTask(void)
{
    switch(rc->mode)
    {
        default:
        case RC_MENU:
        {
            drawMenu(rc->menu);
            break;
        }
        case RC_GAME:
        {
            raycasterGameRenderer();
            break;
        }
        case RC_GAME_OVER:
        {
            raycasterDrawRoundOver();
            break;
        }
        case RC_SCORES:
        {
            raycasterDrawScores();
            break;
        }
    }
    return true;
}

/**
 * This function renders the scene and handles input. It is called as fast as
 * possible by user_main.c's procTask.
 */
void ICACHE_FLASH_ATTR raycasterGameRenderer(void)
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

        // Tick down the timer to display the hurt outline.
        // This is drawn in drawHUD()
        if(rc->gotShotTimer > 0)
        {
            rc->gotShotTimer -= tElapsedUs;
        }
        if(rc->shotSomethingTimer > 0)
        {
            rc->shotSomethingTimer -= tElapsedUs;
        }
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
            if(worldMap[mapX][mapY] <= WMT_C)
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
        int32_t lineHeight;
        if(0 != perpWallDist)
        {
            lineHeight = (int32_t)(OLED_HEIGHT / perpWallDist);
        }
        else
        {
            lineHeight = 0;
        }


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

        // Only draw textures for walls and columns, not empty space or spawn points
        if(worldMap[mapX][mapY] <= WMT_C)
        {
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
            color* wallTex = NULL;
            switch(worldMap[mapX][mapY])
            {
                case WMT_W1:
                {
                    wallTex = rc->sinTex;
                    break;
                }
                case WMT_W2:
                {
                    wallTex = rc->brickTex;
                    break;
                }
                case WMT_W3:
                {
                    wallTex = rc->stripeTex;
                    break;
                }
                case WMT_C:
                {
                    wallTex = rc->stoneTex;
                    break;
                }
                default:
                case WMT_E:
                case WMT_S:
                {
                    // Empty spots don't have textures
                    break;
                }
            }

            if(NULL == wallTex)
            {
                continue;
            }

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
                drawPixelUnsafeC(x, y, wallTex[(texX * texHeight) + texY ]);
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
                    uint16_t texIdx = 0;
                    if(rc->sprites[rc->spriteOrder[i]].mirror)
                    {
                        texIdx = ((texWidth - texX - 1) * texHeight) + texY;
                    }
                    else
                    {
                        texIdx = (texX * texHeight) + texY;
                    }

                    drawPixelUnsafeC(stripe, y, rc->sprites[rc->spriteOrder[i]].texture[texIdx]);

                    if(true == rc->checkShot && (stripe == 63 || stripe == 64))
                    {
                        spriteIdxShot = rc->spriteOrder[i];
                    }
                }
            }
        }
    }

    // If you shot something
    if(spriteIdxShot >= 0)
    {
        // And it's fewer than six units away
        float distSqr = ((rc->sprites[spriteIdxShot].posX - rc->posX) * (rc->sprites[spriteIdxShot].posX - rc->posX)) +
                        ((rc->sprites[spriteIdxShot].posY - rc->posY) * (rc->sprites[spriteIdxShot].posY - rc->posY));
        if(distSqr < 36.0f)
        {
            // And it's not already getting shot or dead
            if(rc->sprites[spriteIdxShot].state != E_GOT_SHOT &&
                    rc->sprites[spriteIdxShot].state != E_DEAD)
            {
                // decrement health by one
                rc->sprites[spriteIdxShot].health--;
                // Animate getting shot
                setSpriteState(&(rc->sprites[spriteIdxShot]), E_GOT_SHOT);
                // Flash LEDs that we shot something
                rc->shotSomethingTimer = LED_ON_TIME;
            }
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
        if(worldMap[(int32_t)(rc->posX + rc->dirX * moveSpeed)][(int32_t)(rc->posY)] > WMT_C)
        {
            rc->posX += rc->dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(rc->posX)][(int32_t)(rc->posY + rc->dirY * moveSpeed)] > WMT_C)
        {
            rc->posY += rc->dirY * moveSpeed;
        }
    }

    // move backwards if no wall behind you
    if(rc->rButtonState & 0x02)
    {
        if(worldMap[(int32_t)(rc->posX - rc->dirX * moveSpeed)][(int32_t)(rc->posY)] > WMT_C)
        {
            rc->posX -= rc->dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(rc->posX)][(int32_t)(rc->posY - rc->dirY * moveSpeed)] > WMT_C)
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

    // Keep track of the closest live sprite
    rc->closestDist = 0xFFFFFFFF;

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

        // Find the distance between this sprite and the player
        float toPlayerX = rc->posX - rc->sprites[i].posX;
        float toPlayerY = rc->posY - rc->sprites[i].posY;
        float magSqr = (toPlayerX * toPlayerX) + (toPlayerY * toPlayerY);

        // Keep track of the closest live sprite
        if(rc->sprites[i].health > 0 && (uint32_t)magSqr < rc->closestDist)
        {
            rc->closestDist = (uint32_t)magSqr;
        }

        switch (rc->sprites[i].state)
        {
            default:
            case E_IDLE:
            {

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
                    rc->sprites[i].texFrame = (rc->sprites[i].texFrame + 1) % NUM_IMP_WALK_FRAMES;
                    rc->sprites[i].texture = rc->impWalk[rc->sprites[i].texFrame];
                    if(0 == rc->sprites[i].texFrame)
                    {
                        rc->sprites[i].mirror = !rc->sprites[i].mirror;
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
                        (worldMap[newPosXi][newPosYi] > WMT_C))
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
                else if(rc->sprites[i].texTimer < SHOOTING_ANIM_TIME / 2 &&
                        rc->sprites[i].shotCooldown <= 0)
                {
                    rc->sprites[i].shotCooldown = ENEMY_SHOT_COOLDOWN;
                    // After 0.5s switch to next texture
                    rc->sprites[i].texture = rc->s2;
                    // Check if the sprite can still see the player
                    if(checkLineToPlayer(&rc->sprites[i], rc->posX, rc->posY))
                    {
                        // TODO RNG if damage actually lands?
                        rc->health--;
                        rc->gotShotTimer = LED_ON_TIME;
                        if(rc->health == 0)
                        {
                            raycasterEndRound();
                        }
                    }
                }
                break;
            }
            case E_GOT_SHOT:
            {
                // Animate getting shot
                rc->sprites[i].texTimer -= tElapsedUs;
                if(rc->sprites[i].texTimer <= 0)
                {
                    // If the enemy has any health
                    if(rc->sprites[i].health > 0)
                    {
                        // Go back to E_PICK_DIR_PLAYER
                        setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                    }
                    else
                    {
                        // If there is no health, go to E_DEAD
                        setSpriteState(&(rc->sprites[i]), E_DEAD);
                    }
                }
                else if(rc->sprites[i].texTimer < GOT_SHOT_ANIM_TIME / 2)
                {
                    // After 0.5s switch to next texture
                    rc->sprites[i].texture = rc->d2;
                }
                break;
            }
            case E_DEAD:
            {
                // Do nothing, ya dead
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
        if(worldMap[mapX][mapY] <= WMT_C)
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
    // See if the sprite had been in a walking state
    bool wasWalking;
    switch(sprite->state)
    {
        default:
        case E_IDLE:
        case E_PICK_DIR_PLAYER:
        case E_PICK_DIR_RAND:
        case E_WALKING:
        {
            wasWalking = true;
            break;
        }
        case E_SHOOTING:
        case E_GOT_SHOT:
        case E_DEAD:
        {
            wasWalking = false;
        }
    }

    // Set the state
    sprite->state = state;
    sprite->texFrame = 0;

    // Set timers and textures
    switch(state)
    {
        default:
        case E_IDLE:
        case E_PICK_DIR_PLAYER:
        case E_PICK_DIR_RAND:
        {
            sprite->stateTimer = 0;
            if(!wasWalking || NULL == sprite->texture)
            {
                sprite->texture = rc->impWalk[0];
                sprite->mirror = false;
                sprite->texTimer = 0;
            }
            break;
        }
        case E_WALKING:
        {
            sprite->stateTimer = WALK_ANIM_TIME;
            if(!wasWalking || NULL == sprite->texture)
            {
                sprite->texture = rc->impWalk[0];
                sprite->mirror = false;
                sprite->texTimer = STEP_ANIM_TIME;
            }
            break;
        }
        case E_SHOOTING:
        {
            sprite->stateTimer = SHOOTING_ANIM_TIME;
            sprite->texture = rc->s1;
            sprite->texTimer = SHOOTING_ANIM_TIME;
            break;
        }
        case E_GOT_SHOT:
        {
            sprite->stateTimer = GOT_SHOT_ANIM_TIME;
            sprite->texture = rc->d1;
            sprite->texTimer = GOT_SHOT_ANIM_TIME;
            break;
        }
        case E_DEAD:
        {
            rc->liveSprites--;
            rc->kills++;
            if(0 == rc->liveSprites)
            {
                raycasterEndRound();
            }
            sprite->stateTimer = 0;
            sprite->texture = rc->d3;
            sprite->texTimer = 0;
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
    ets_snprintf(notes, sizeof(notes) - 1, "%d", rc->liveSprites);
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
    ets_snprintf(health, sizeof(health) - 1, "%d", rc->health);
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

/**
 * Called when the round ends
 */
void ICACHE_FLASH_ATTR raycasterEndRound(void)
{
    rc->tRoundElapsed = system_get_time() - rc->tRoundStartedUs;

    // TODO save score
    rc->mode = RC_GAME_OVER;
    // Disable radar
    rc->closestDist = 0xFFFFFFFF;
}

/**
 * Display the time elapsed and number of kills
 * TODO make this pretty
 */
void ICACHE_FLASH_ATTR raycasterDrawRoundOver(void)
{
    uint32_t dSec = (rc->tRoundStartedUs / 100000) % 10;
    uint32_t sec  = (rc->tRoundStartedUs / 1000000) % 60;
    uint32_t min  = (rc->tRoundStartedUs / (1000000 * 60));

    char timestr[64] = {0};
    ets_snprintf(timestr, sizeof(timestr), "Time : %02d:%02d.%d", min, sec, dSec);
    char killstr[64] = {0};
    ets_snprintf(killstr, sizeof(killstr), "Kills: %d", rc->kills);

    clearDisplay();

    if(rc->liveSprites > 0)
    {
        plotText(0, 0, "Game Over", IBM_VGA_8, WHITE);
    }
    else
    {
        plotText(0, 0, "You win!", IBM_VGA_8, WHITE);
    }

    plotText(0, FONT_HEIGHT_IBMVGA8 + 2, timestr, IBM_VGA_8, WHITE);
    plotText(0, 2 * (FONT_HEIGHT_IBMVGA8 + 2), killstr, IBM_VGA_8, WHITE);
}

/**
 * @brief Drive LEDs based on game state
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR raycasterLedTimer(void* arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};

    // If we were shot, flash red
    if(rc->gotShotTimer > 0)
    {
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].r = (rc->gotShotTimer * 0x40) / LED_ON_TIME;
        }
    }
    // Otherwise if we shot somethhing, flash green
    else if(rc->shotSomethingTimer > 0)
    {
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].g = (rc->shotSomethingTimer * 0x40) / LED_ON_TIME;
        }
    }
    // Otherwise use the LEDs like a radar
    else if(rc->closestDist < 64) // This is the squared dist, so check for radius 8
    {
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].b = 3 * (64 - rc->closestDist);
        }
    }

    // Push out the LEDs
    setLeds(leds, sizeof(leds));
}

/**
 * Display the high scores
 */
void ICACHE_FLASH_ATTR raycasterDrawScores(void)
{
    clearDisplay();
    plotText(0, 0, "Scores", IBM_VGA_8, WHITE);
    // TODO display scores
}
