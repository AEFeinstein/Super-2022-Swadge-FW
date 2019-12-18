/*
 * mode_gallery.c
 *
 *  Created on: Oct 13, 2019
 *      Author: adam
 */

/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>

#include "user_main.h"
#include "oled.h"
#include "mode_gallery.h"
#include "galleryImages.h"
#include "fastlz.h"
#include "font.h"
#include "custom_commands.h"
#include "buzzer.h"
#include "hpatimer.h"
#include "bresenham.h"
#include "mode_tiltrads.h"
#include "songs.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define MAX_DECOMPRESSED_SIZE 0xA00

/*
#define DBG_GAL(...) do { \
        os_printf("%s::%d ", __func__, __LINE__); \
        os_printf(__VA_ARGS__); \
    } while(0)
*/
#define DBG_GAL(...)

/*==============================================================================
 * Enums
 *============================================================================*/

typedef enum
{
    RIGHT,
    LEFT
} panDir_t;

typedef enum
{
    NONE,
    ALWAYS_RIGHT,
    ALWAYS_LEFT
} panContDir_t;

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    const uint8_t* data;
    const uint16_t len;
} galFrame_t;

typedef struct
{
    const uint8_t nFrames;
    const panContDir_t continousPan;
    const galFrame_t frames[];
} galImage_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR galEnterMode(void);
void ICACHE_FLASH_ATTR galExitMode(void);
void ICACHE_FLASH_ATTR galButtonCallback(uint8_t state, int button, int down);
void ICACHE_FLASH_ATTR galLoadFirstFrame(void);
void ICACHE_FLASH_ATTR galClearImage(void);
void ICACHE_FLASH_ATTR galDrawFrame(void);
static void ICACHE_FLASH_ATTR galLoadNextFrame(void* arg);
static void ICACHE_FLASH_ATTR galTimerPan(void* arg);
static void ICACHE_FLASH_ATTR galTimerMusic(void* arg);
const galImage_t* ICACHE_FLASH_ATTR galGetCurrentImage(void);
bool ICACHE_FLASH_ATTR galIsImageUnlocked(void);
void ICACHE_FLASH_ATTR galRearmMusicTimer(void);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode galleryMode =
{
    .modeName = "gallery",
    .fnEnterMode = galEnterMode,
    .fnExitMode = galExitMode,
    .fnButtonCallback = galButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .menuImageData = mnu_gallery_0,
    .menuImageLen = sizeof(mnu_gallery_0)
};

struct
{
    uint8_t* compressedData;   ///< A pointer to compressed data in RAM
    uint8_t* decompressedData; ///< A pointer to decompressed data in RAM
    uint8_t* frameData;        ///< A pointer to the frame being displayed
    uint16_t width;            ///< The width of the current image
    uint16_t virtualWidth;     ///< The effective width = actual width or double that if continous panning
    uint16_t height;           ///< The height of the current image
    uint16_t nFrames;          ///< The number of frames in the image
    uint16_t cFrame;           ///< The current frame index being displyed
    uint16_t durationMs;       ///< The duration each frame should be displayed
    os_timer_t timerAnimate;   ///< A timer to animate the image
    os_timer_t timerPan;       ///< A timer to pan the image
    os_timer_t timerMusic;     ///< A timer to start music
    uint16_t cImage;           ///< The index of the current image being
    uint16_t panIdx;           ///< How much the image is currently panned
    panDir_t panDir;           ///< The direction the image is currently panning
    uint32_t unlockBitmask;    ///< A bitmask of the unlocked gallery images
    uint8_t h;
    bool u;
} gal =
{
    .compressedData = NULL,
    .decompressedData = NULL,
    .frameData = NULL,
    .width = 0,
    .height = 0,
    .nFrames = 0,
    .cFrame = 0,
    .durationMs = 0,
    .timerAnimate = {0},
    .timerPan = {0},
    .timerMusic = {0},
    .cImage = 0,
    .panIdx = 0,
    .panDir = RIGHT
};

/*==============================================================================
 * Const Variables
 *============================================================================*/

const galImage_t galBongo =
{
    .nFrames = 2,
    .continousPan = NONE,
    .frames = {
        {.data = gal_bongo_0, .len = sizeof(gal_bongo_0)},
        {.data = gal_bongo_1, .len = sizeof(gal_bongo_1)},
    }
};

const galImage_t galSnort =
{
    .nFrames = 1,
    .continousPan = ALWAYS_LEFT,
    .frames = {
        {.data = gal_snort_0, .len = sizeof(gal_snort_0)},
    }
};

const galImage_t galGaylord =
{
    .nFrames = 3,
    .continousPan = NONE,
    .frames = {
        {.data = gal_gaylord_0, .len = sizeof(gal_gaylord_0)},
        {.data = gal_gaylord_1, .len = sizeof(gal_gaylord_1)},
        {.data = gal_gaylord_2, .len = sizeof(gal_gaylord_2)},
    }
};

const galImage_t galFunkus =
{
    .nFrames = 30,
    .continousPan = NONE,
    .frames = {
        {.data = gal_funkus_0, .len = sizeof(gal_funkus_0)},
        {.data = gal_funkus_1, .len = sizeof(gal_funkus_1)},
        {.data = gal_funkus_2, .len = sizeof(gal_funkus_2)},
        {.data = gal_funkus_3, .len = sizeof(gal_funkus_3)},
        {.data = gal_funkus_4, .len = sizeof(gal_funkus_4)},
        {.data = gal_funkus_5, .len = sizeof(gal_funkus_5)},
        {.data = gal_funkus_6, .len = sizeof(gal_funkus_6)},
        {.data = gal_funkus_7, .len = sizeof(gal_funkus_7)},
        {.data = gal_funkus_8, .len = sizeof(gal_funkus_8)},
        {.data = gal_funkus_9, .len = sizeof(gal_funkus_9)},
        {.data = gal_funkus_10, .len = sizeof(gal_funkus_10)},
        {.data = gal_funkus_11, .len = sizeof(gal_funkus_11)},
        {.data = gal_funkus_12, .len = sizeof(gal_funkus_12)},
        {.data = gal_funkus_13, .len = sizeof(gal_funkus_13)},
        {.data = gal_funkus_14, .len = sizeof(gal_funkus_14)},
        {.data = gal_funkus_15, .len = sizeof(gal_funkus_15)},
        {.data = gal_funkus_16, .len = sizeof(gal_funkus_16)},
        {.data = gal_funkus_17, .len = sizeof(gal_funkus_17)},
        {.data = gal_funkus_18, .len = sizeof(gal_funkus_18)},
        {.data = gal_funkus_19, .len = sizeof(gal_funkus_19)},
        {.data = gal_funkus_20, .len = sizeof(gal_funkus_20)},
        {.data = gal_funkus_21, .len = sizeof(gal_funkus_21)},
        {.data = gal_funkus_22, .len = sizeof(gal_funkus_22)},
        {.data = gal_funkus_23, .len = sizeof(gal_funkus_23)},
        {.data = gal_funkus_24, .len = sizeof(gal_funkus_24)},
        {.data = gal_funkus_25, .len = sizeof(gal_funkus_25)},
        {.data = gal_funkus_26, .len = sizeof(gal_funkus_26)},
        {.data = gal_funkus_27, .len = sizeof(gal_funkus_27)},
        {.data = gal_funkus_28, .len = sizeof(gal_funkus_28)},
        {.data = gal_funkus_29, .len = sizeof(gal_funkus_29)},
    }
};

const galImage_t galLogo =
{
    .nFrames = 21,
    .continousPan = NONE,
    .frames = {
        {.data = gal_logo_0, .len = sizeof(gal_logo_0)},
        {.data = gal_logo_1, .len = sizeof(gal_logo_1)},
        {.data = gal_logo_2, .len = sizeof(gal_logo_2)},
        {.data = gal_logo_3, .len = sizeof(gal_logo_3)},
        {.data = gal_logo_4, .len = sizeof(gal_logo_4)},
        {.data = gal_logo_5, .len = sizeof(gal_logo_5)},
        {.data = gal_logo_6, .len = sizeof(gal_logo_6)},
        {.data = gal_logo_7, .len = sizeof(gal_logo_7)},
        {.data = gal_logo_8, .len = sizeof(gal_logo_8)},
        {.data = gal_logo_9, .len = sizeof(gal_logo_9)},
        {.data = gal_logo_10, .len = sizeof(gal_logo_10)},
        {.data = gal_logo_11, .len = sizeof(gal_logo_11)},
        {.data = gal_logo_12, .len = sizeof(gal_logo_12)},
        {.data = gal_logo_13, .len = sizeof(gal_logo_13)},
        {.data = gal_logo_14, .len = sizeof(gal_logo_14)},
        {.data = gal_logo_15, .len = sizeof(gal_logo_15)},
        {.data = gal_logo_16, .len = sizeof(gal_logo_16)},
        {.data = gal_logo_17, .len = sizeof(gal_logo_17)},
        {.data = gal_logo_18, .len = sizeof(gal_logo_18)},
        {.data = gal_logo_19, .len = sizeof(gal_logo_19)},
        {.data = gal_logo_20, .len = sizeof(gal_logo_20)},
    }
};

const galImage_t galUnlockJoust =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_joust_0, .len = sizeof(gal_unlock_joust_0)},
    }
};

const galImage_t galUnlockMaze =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_maze_0, .len = sizeof(gal_unlock_maze_0)},
    }
};

const galImage_t galUnlockTiltrads =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_tiltrads_0, .len = sizeof(gal_unlock_tiltrads_0)},
    }
};

const galImage_t galUnlockSnake =
{
    .nFrames = 1,
    .frames = {
        {.data = gal_unlock_snake_0, .len = sizeof(gal_unlock_snake_0)},
    }
};

const galImage_t allabout =
{
    .nFrames = 105,
    .continousPan = NONE,
    .frames = {
        {.data = gal_allabout_00, .len = sizeof(gal_allabout_00)},
        {.data = gal_allabout_01, .len = sizeof(gal_allabout_01)},
        {.data = gal_allabout_02, .len = sizeof(gal_allabout_02)},
        {.data = gal_allabout_03, .len = sizeof(gal_allabout_03)},
        {.data = gal_allabout_04, .len = sizeof(gal_allabout_04)},
        {.data = gal_allabout_05, .len = sizeof(gal_allabout_05)},
        {.data = gal_allabout_06, .len = sizeof(gal_allabout_06)},
        {.data = gal_allabout_07, .len = sizeof(gal_allabout_07)},
        {.data = gal_allabout_08, .len = sizeof(gal_allabout_08)},
        {.data = gal_allabout_09, .len = sizeof(gal_allabout_09)},
        {.data = gal_allabout_10, .len = sizeof(gal_allabout_10)},
        {.data = gal_allabout_11, .len = sizeof(gal_allabout_11)},
        {.data = gal_allabout_12, .len = sizeof(gal_allabout_12)},
        {.data = gal_allabout_13, .len = sizeof(gal_allabout_13)},
        {.data = gal_allabout_14, .len = sizeof(gal_allabout_14)},
        {.data = gal_allabout_15, .len = sizeof(gal_allabout_15)},
        {.data = gal_allabout_16, .len = sizeof(gal_allabout_16)},
        {.data = gal_allabout_17, .len = sizeof(gal_allabout_17)},
        {.data = gal_allabout_18, .len = sizeof(gal_allabout_18)},
        {.data = gal_allabout_19, .len = sizeof(gal_allabout_19)},
        {.data = gal_allabout_20, .len = sizeof(gal_allabout_20)},
        {.data = gal_allabout_21, .len = sizeof(gal_allabout_21)},
        {.data = gal_allabout_22, .len = sizeof(gal_allabout_22)},
        {.data = gal_allabout_23, .len = sizeof(gal_allabout_23)},
        {.data = gal_allabout_24, .len = sizeof(gal_allabout_24)},
        {.data = gal_allabout_25, .len = sizeof(gal_allabout_25)},
        {.data = gal_allabout_26, .len = sizeof(gal_allabout_26)},
        {.data = gal_allabout_27, .len = sizeof(gal_allabout_27)},
        {.data = gal_allabout_28, .len = sizeof(gal_allabout_28)},
        {.data = gal_allabout_29, .len = sizeof(gal_allabout_29)},
        {.data = gal_allabout_30, .len = sizeof(gal_allabout_30)},
        {.data = gal_allabout_31, .len = sizeof(gal_allabout_31)},
        {.data = gal_allabout_32, .len = sizeof(gal_allabout_32)},
        {.data = gal_allabout_33, .len = sizeof(gal_allabout_33)},
        {.data = gal_allabout_34, .len = sizeof(gal_allabout_34)},
        {.data = gal_allabout_35, .len = sizeof(gal_allabout_35)},
        {.data = gal_allabout_36, .len = sizeof(gal_allabout_36)},
        {.data = gal_allabout_37, .len = sizeof(gal_allabout_37)},
        {.data = gal_allabout_38, .len = sizeof(gal_allabout_38)},
        {.data = gal_allabout_39, .len = sizeof(gal_allabout_39)},
        {.data = gal_allabout_40, .len = sizeof(gal_allabout_40)},
        {.data = gal_allabout_41, .len = sizeof(gal_allabout_41)},
        {.data = gal_allabout_42, .len = sizeof(gal_allabout_42)},
        {.data = gal_allabout_43, .len = sizeof(gal_allabout_43)},
        {.data = gal_allabout_44, .len = sizeof(gal_allabout_44)},
        {.data = gal_allabout_45, .len = sizeof(gal_allabout_45)},
        {.data = gal_allabout_46, .len = sizeof(gal_allabout_46)},
        {.data = gal_allabout_47, .len = sizeof(gal_allabout_47)},
        {.data = gal_allabout_48, .len = sizeof(gal_allabout_48)},
        {.data = gal_allabout_49, .len = sizeof(gal_allabout_49)},
        {.data = gal_allabout_50, .len = sizeof(gal_allabout_50)},
        {.data = gal_allabout_51, .len = sizeof(gal_allabout_51)},
        {.data = gal_allabout_52, .len = sizeof(gal_allabout_52)},
        {.data = gal_allabout_53, .len = sizeof(gal_allabout_53)},
        {.data = gal_allabout_54, .len = sizeof(gal_allabout_54)},
        {.data = gal_allabout_55, .len = sizeof(gal_allabout_55)},
        {.data = gal_allabout_56, .len = sizeof(gal_allabout_56)},
        {.data = gal_allabout_57, .len = sizeof(gal_allabout_57)},
        {.data = gal_allabout_58, .len = sizeof(gal_allabout_58)},
        {.data = gal_allabout_59, .len = sizeof(gal_allabout_59)},
        {.data = gal_allabout_60, .len = sizeof(gal_allabout_60)},
        {.data = gal_allabout_61, .len = sizeof(gal_allabout_61)},
        {.data = gal_allabout_62, .len = sizeof(gal_allabout_62)},
        {.data = gal_allabout_63, .len = sizeof(gal_allabout_63)},
        {.data = gal_allabout_64, .len = sizeof(gal_allabout_64)},
        {.data = gal_allabout_65, .len = sizeof(gal_allabout_65)},
        {.data = gal_allabout_66, .len = sizeof(gal_allabout_66)},
        {.data = gal_allabout_67, .len = sizeof(gal_allabout_67)},
        {.data = gal_allabout_68, .len = sizeof(gal_allabout_68)},
        {.data = gal_allabout_69, .len = sizeof(gal_allabout_69)},
        {.data = gal_allabout_70, .len = sizeof(gal_allabout_70)},
        {.data = gal_allabout_71, .len = sizeof(gal_allabout_71)},
        {.data = gal_allabout_72, .len = sizeof(gal_allabout_72)},
        {.data = gal_allabout_73, .len = sizeof(gal_allabout_73)},
        {.data = gal_allabout_74, .len = sizeof(gal_allabout_74)},
        {.data = gal_allabout_75, .len = sizeof(gal_allabout_75)},
        {.data = gal_allabout_76, .len = sizeof(gal_allabout_76)},
        {.data = gal_allabout_77, .len = sizeof(gal_allabout_77)},
        {.data = gal_allabout_78, .len = sizeof(gal_allabout_78)},
        {.data = gal_allabout_79, .len = sizeof(gal_allabout_79)},
        {.data = gal_allabout_80, .len = sizeof(gal_allabout_80)},
        {.data = gal_allabout_81, .len = sizeof(gal_allabout_81)},
        {.data = gal_allabout_82, .len = sizeof(gal_allabout_82)},
        {.data = gal_allabout_83, .len = sizeof(gal_allabout_83)},
        {.data = gal_allabout_84, .len = sizeof(gal_allabout_84)},
        {.data = gal_allabout_85, .len = sizeof(gal_allabout_85)},
        {.data = gal_allabout_86, .len = sizeof(gal_allabout_86)},
        {.data = gal_allabout_87, .len = sizeof(gal_allabout_87)},
        {.data = gal_allabout_88, .len = sizeof(gal_allabout_88)},
        {.data = gal_allabout_89, .len = sizeof(gal_allabout_89)},
        {.data = gal_allabout_90, .len = sizeof(gal_allabout_90)},
        {.data = gal_allabout_91, .len = sizeof(gal_allabout_91)},
        {.data = gal_allabout_92, .len = sizeof(gal_allabout_92)},
        {.data = gal_allabout_93, .len = sizeof(gal_allabout_93)},
        {.data = gal_allabout_94, .len = sizeof(gal_allabout_94)},
        {.data = gal_allabout_95, .len = sizeof(gal_allabout_95)},
        {.data = gal_allabout_96, .len = sizeof(gal_allabout_96)},
        {.data = gal_allabout_97, .len = sizeof(gal_allabout_97)},
        {.data = gal_allabout_98, .len = sizeof(gal_allabout_98)},
        {.data = gal_allabout_99, .len = sizeof(gal_allabout_99)},
        {.data = gal_allabout_100, .len = sizeof(gal_allabout_100)},
        {.data = gal_allabout_101, .len = sizeof(gal_allabout_101)},
        {.data = gal_allabout_102, .len = sizeof(gal_allabout_102)},
        {.data = gal_allabout_103, .len = sizeof(gal_allabout_103)},
        {.data = gal_allabout_104, .len = sizeof(gal_allabout_104)},
    }
};

const galImage_t bananas_animals =
{
    .nFrames = 9,
    .continousPan = NONE,
    .frames = {
        {.data = gal_bananas_animals_00, .len = sizeof(gal_bananas_animals_00)},
        {.data = gal_bananas_animals_01, .len = sizeof(gal_bananas_animals_01)},
        {.data = gal_bananas_animals_02, .len = sizeof(gal_bananas_animals_02)},
        {.data = gal_bananas_animals_03, .len = sizeof(gal_bananas_animals_03)},
        {.data = gal_bananas_animals_04, .len = sizeof(gal_bananas_animals_04)},
        {.data = gal_bananas_animals_05, .len = sizeof(gal_bananas_animals_05)},
        {.data = gal_bananas_animals_06, .len = sizeof(gal_bananas_animals_06)},
        {.data = gal_bananas_animals_07, .len = sizeof(gal_bananas_animals_07)},
        {.data = gal_bananas_animals_08, .len = sizeof(gal_bananas_animals_08)},
    }
};

const galImage_t colossus =
{
    .nFrames = 75,
    .continousPan = NONE,
    .frames = {
        {.data = gal_colossus_00, .len = sizeof(gal_colossus_00)},
        {.data = gal_colossus_01, .len = sizeof(gal_colossus_01)},
        {.data = gal_colossus_02, .len = sizeof(gal_colossus_02)},
        {.data = gal_colossus_03, .len = sizeof(gal_colossus_03)},
        {.data = gal_colossus_04, .len = sizeof(gal_colossus_04)},
        {.data = gal_colossus_05, .len = sizeof(gal_colossus_05)},
        {.data = gal_colossus_06, .len = sizeof(gal_colossus_06)},
        {.data = gal_colossus_07, .len = sizeof(gal_colossus_07)},
        {.data = gal_colossus_08, .len = sizeof(gal_colossus_08)},
        {.data = gal_colossus_09, .len = sizeof(gal_colossus_09)},
        {.data = gal_colossus_10, .len = sizeof(gal_colossus_10)},
        {.data = gal_colossus_11, .len = sizeof(gal_colossus_11)},
        {.data = gal_colossus_12, .len = sizeof(gal_colossus_12)},
        {.data = gal_colossus_13, .len = sizeof(gal_colossus_13)},
        {.data = gal_colossus_14, .len = sizeof(gal_colossus_14)},
        {.data = gal_colossus_15, .len = sizeof(gal_colossus_15)},
        {.data = gal_colossus_16, .len = sizeof(gal_colossus_16)},
        {.data = gal_colossus_17, .len = sizeof(gal_colossus_17)},
        {.data = gal_colossus_18, .len = sizeof(gal_colossus_18)},
        {.data = gal_colossus_19, .len = sizeof(gal_colossus_19)},
        {.data = gal_colossus_20, .len = sizeof(gal_colossus_20)},
        {.data = gal_colossus_21, .len = sizeof(gal_colossus_21)},
        {.data = gal_colossus_22, .len = sizeof(gal_colossus_22)},
        {.data = gal_colossus_23, .len = sizeof(gal_colossus_23)},
        {.data = gal_colossus_24, .len = sizeof(gal_colossus_24)},
        {.data = gal_colossus_25, .len = sizeof(gal_colossus_25)},
        {.data = gal_colossus_26, .len = sizeof(gal_colossus_26)},
        {.data = gal_colossus_27, .len = sizeof(gal_colossus_27)},
        {.data = gal_colossus_28, .len = sizeof(gal_colossus_28)},
        {.data = gal_colossus_29, .len = sizeof(gal_colossus_29)},
        {.data = gal_colossus_30, .len = sizeof(gal_colossus_30)},
        {.data = gal_colossus_31, .len = sizeof(gal_colossus_31)},
        {.data = gal_colossus_32, .len = sizeof(gal_colossus_32)},
        {.data = gal_colossus_33, .len = sizeof(gal_colossus_33)},
        {.data = gal_colossus_34, .len = sizeof(gal_colossus_34)},
        {.data = gal_colossus_35, .len = sizeof(gal_colossus_35)},
        {.data = gal_colossus_36, .len = sizeof(gal_colossus_36)},
        {.data = gal_colossus_37, .len = sizeof(gal_colossus_37)},
        {.data = gal_colossus_38, .len = sizeof(gal_colossus_38)},
        {.data = gal_colossus_39, .len = sizeof(gal_colossus_39)},
        {.data = gal_colossus_40, .len = sizeof(gal_colossus_40)},
        {.data = gal_colossus_41, .len = sizeof(gal_colossus_41)},
        {.data = gal_colossus_42, .len = sizeof(gal_colossus_42)},
        {.data = gal_colossus_43, .len = sizeof(gal_colossus_43)},
        {.data = gal_colossus_44, .len = sizeof(gal_colossus_44)},
        {.data = gal_colossus_45, .len = sizeof(gal_colossus_45)},
        {.data = gal_colossus_46, .len = sizeof(gal_colossus_46)},
        {.data = gal_colossus_47, .len = sizeof(gal_colossus_47)},
        {.data = gal_colossus_48, .len = sizeof(gal_colossus_48)},
        {.data = gal_colossus_49, .len = sizeof(gal_colossus_49)},
        {.data = gal_colossus_50, .len = sizeof(gal_colossus_50)},
        {.data = gal_colossus_51, .len = sizeof(gal_colossus_51)},
        {.data = gal_colossus_52, .len = sizeof(gal_colossus_52)},
        {.data = gal_colossus_53, .len = sizeof(gal_colossus_53)},
        {.data = gal_colossus_54, .len = sizeof(gal_colossus_54)},
        {.data = gal_colossus_55, .len = sizeof(gal_colossus_55)},
        {.data = gal_colossus_56, .len = sizeof(gal_colossus_56)},
        {.data = gal_colossus_57, .len = sizeof(gal_colossus_57)},
        {.data = gal_colossus_58, .len = sizeof(gal_colossus_58)},
        {.data = gal_colossus_59, .len = sizeof(gal_colossus_59)},
        {.data = gal_colossus_60, .len = sizeof(gal_colossus_60)},
        {.data = gal_colossus_61, .len = sizeof(gal_colossus_61)},
        {.data = gal_colossus_62, .len = sizeof(gal_colossus_62)},
        {.data = gal_colossus_63, .len = sizeof(gal_colossus_63)},
        {.data = gal_colossus_64, .len = sizeof(gal_colossus_64)},
        {.data = gal_colossus_65, .len = sizeof(gal_colossus_65)},
        {.data = gal_colossus_66, .len = sizeof(gal_colossus_66)},
        {.data = gal_colossus_67, .len = sizeof(gal_colossus_67)},
        {.data = gal_colossus_68, .len = sizeof(gal_colossus_68)},
        {.data = gal_colossus_69, .len = sizeof(gal_colossus_69)},
        {.data = gal_colossus_70, .len = sizeof(gal_colossus_70)},
        {.data = gal_colossus_71, .len = sizeof(gal_colossus_71)},
        {.data = gal_colossus_72, .len = sizeof(gal_colossus_72)},
        {.data = gal_colossus_73, .len = sizeof(gal_colossus_73)},
        {.data = gal_colossus_74, .len = sizeof(gal_colossus_74)},
    }
};

const galImage_t frank =
{
    .nFrames = 9,
    .continousPan = NONE,
    .frames = {
        {.data = gal_frank_00, .len = sizeof(gal_frank_00)},
        {.data = gal_frank_01, .len = sizeof(gal_frank_01)},
        {.data = gal_frank_02, .len = sizeof(gal_frank_02)},
        {.data = gal_frank_03, .len = sizeof(gal_frank_03)},
        {.data = gal_frank_04, .len = sizeof(gal_frank_04)},
        {.data = gal_frank_05, .len = sizeof(gal_frank_05)},
        {.data = gal_frank_06, .len = sizeof(gal_frank_06)},
        {.data = gal_frank_07, .len = sizeof(gal_frank_07)},
        {.data = gal_frank_08, .len = sizeof(gal_frank_08)},
    }
};

const galImage_t funky =
{
    .nFrames = 40,
    .continousPan = NONE,
    .frames = {
        {.data = gal_funky_00, .len = sizeof(gal_funky_00)},
        {.data = gal_funky_01, .len = sizeof(gal_funky_01)},
        {.data = gal_funky_02, .len = sizeof(gal_funky_02)},
        {.data = gal_funky_03, .len = sizeof(gal_funky_03)},
        {.data = gal_funky_04, .len = sizeof(gal_funky_04)},
        {.data = gal_funky_05, .len = sizeof(gal_funky_05)},
        {.data = gal_funky_06, .len = sizeof(gal_funky_06)},
        {.data = gal_funky_07, .len = sizeof(gal_funky_07)},
        {.data = gal_funky_08, .len = sizeof(gal_funky_08)},
        {.data = gal_funky_09, .len = sizeof(gal_funky_09)},
        {.data = gal_funky_10, .len = sizeof(gal_funky_10)},
        {.data = gal_funky_11, .len = sizeof(gal_funky_11)},
        {.data = gal_funky_12, .len = sizeof(gal_funky_12)},
        {.data = gal_funky_13, .len = sizeof(gal_funky_13)},
        {.data = gal_funky_14, .len = sizeof(gal_funky_14)},
        {.data = gal_funky_15, .len = sizeof(gal_funky_15)},
        {.data = gal_funky_16, .len = sizeof(gal_funky_16)},
        {.data = gal_funky_17, .len = sizeof(gal_funky_17)},
        {.data = gal_funky_18, .len = sizeof(gal_funky_18)},
        {.data = gal_funky_19, .len = sizeof(gal_funky_19)},
        {.data = gal_funky_20, .len = sizeof(gal_funky_20)},
        {.data = gal_funky_21, .len = sizeof(gal_funky_21)},
        {.data = gal_funky_22, .len = sizeof(gal_funky_22)},
        {.data = gal_funky_23, .len = sizeof(gal_funky_23)},
        {.data = gal_funky_24, .len = sizeof(gal_funky_24)},
        {.data = gal_funky_25, .len = sizeof(gal_funky_25)},
        {.data = gal_funky_26, .len = sizeof(gal_funky_26)},
        {.data = gal_funky_27, .len = sizeof(gal_funky_27)},
        {.data = gal_funky_28, .len = sizeof(gal_funky_28)},
        {.data = gal_funky_29, .len = sizeof(gal_funky_29)},
        {.data = gal_funky_30, .len = sizeof(gal_funky_30)},
        {.data = gal_funky_31, .len = sizeof(gal_funky_31)},
        {.data = gal_funky_32, .len = sizeof(gal_funky_32)},
        {.data = gal_funky_33, .len = sizeof(gal_funky_33)},
        {.data = gal_funky_34, .len = sizeof(gal_funky_34)},
        {.data = gal_funky_35, .len = sizeof(gal_funky_35)},
        {.data = gal_funky_36, .len = sizeof(gal_funky_36)},
        {.data = gal_funky_37, .len = sizeof(gal_funky_37)},
        {.data = gal_funky_38, .len = sizeof(gal_funky_38)},
        {.data = gal_funky_39, .len = sizeof(gal_funky_39)},
    }
};

const galImage_t josiah =
{
    .nFrames = 10,
    .continousPan = NONE,
    .frames = {
        {.data = gal_josiah_00, .len = sizeof(gal_josiah_00)},
        {.data = gal_josiah_01, .len = sizeof(gal_josiah_01)},
        {.data = gal_josiah_02, .len = sizeof(gal_josiah_02)},
        {.data = gal_josiah_03, .len = sizeof(gal_josiah_03)},
        {.data = gal_josiah_04, .len = sizeof(gal_josiah_04)},
        {.data = gal_josiah_05, .len = sizeof(gal_josiah_05)},
        {.data = gal_josiah_06, .len = sizeof(gal_josiah_06)},
        {.data = gal_josiah_07, .len = sizeof(gal_josiah_07)},
        {.data = gal_josiah_08, .len = sizeof(gal_josiah_08)},
        {.data = gal_josiah_09, .len = sizeof(gal_josiah_09)},
    }
};

const galImage_t magsquare =
{
    .nFrames = 4,
    .continousPan = NONE,
    .frames = {
        {.data = gal_magsquare_00, .len = sizeof(gal_magsquare_00)},
        {.data = gal_magsquare_01, .len = sizeof(gal_magsquare_01)},
        {.data = gal_magsquare_02, .len = sizeof(gal_magsquare_02)},
        {.data = gal_magsquare_03, .len = sizeof(gal_magsquare_03)},
    }
};

const galImage_t magvania =
{
    .nFrames = 16,
    .continousPan = NONE,
    .frames = {
        {.data = gal_magvania_00, .len = sizeof(gal_magvania_00)},
        {.data = gal_magvania_01, .len = sizeof(gal_magvania_01)},
        {.data = gal_magvania_02, .len = sizeof(gal_magvania_02)},
        {.data = gal_magvania_03, .len = sizeof(gal_magvania_03)},
        {.data = gal_magvania_04, .len = sizeof(gal_magvania_04)},
        {.data = gal_magvania_05, .len = sizeof(gal_magvania_05)},
        {.data = gal_magvania_06, .len = sizeof(gal_magvania_06)},
        {.data = gal_magvania_07, .len = sizeof(gal_magvania_07)},
        {.data = gal_magvania_08, .len = sizeof(gal_magvania_08)},
        {.data = gal_magvania_09, .len = sizeof(gal_magvania_09)},
        {.data = gal_magvania_10, .len = sizeof(gal_magvania_10)},
        {.data = gal_magvania_11, .len = sizeof(gal_magvania_11)},
        {.data = gal_magvania_12, .len = sizeof(gal_magvania_12)},
        {.data = gal_magvania_13, .len = sizeof(gal_magvania_13)},
        {.data = gal_magvania_14, .len = sizeof(gal_magvania_14)},
        {.data = gal_magvania_15, .len = sizeof(gal_magvania_15)},
    }
};

const galImage_t mivs =
{
    .nFrames = 20,
    .continousPan = NONE,
    .frames = {
        {.data = gal_mivs_00, .len = sizeof(gal_mivs_00)},
        {.data = gal_mivs_01, .len = sizeof(gal_mivs_01)},
        {.data = gal_mivs_02, .len = sizeof(gal_mivs_02)},
        {.data = gal_mivs_03, .len = sizeof(gal_mivs_03)},
        {.data = gal_mivs_04, .len = sizeof(gal_mivs_04)},
        {.data = gal_mivs_05, .len = sizeof(gal_mivs_05)},
        {.data = gal_mivs_06, .len = sizeof(gal_mivs_06)},
        {.data = gal_mivs_07, .len = sizeof(gal_mivs_07)},
        {.data = gal_mivs_08, .len = sizeof(gal_mivs_08)},
        {.data = gal_mivs_09, .len = sizeof(gal_mivs_09)},
        {.data = gal_mivs_10, .len = sizeof(gal_mivs_10)},
        {.data = gal_mivs_11, .len = sizeof(gal_mivs_11)},
        {.data = gal_mivs_12, .len = sizeof(gal_mivs_12)},
        {.data = gal_mivs_13, .len = sizeof(gal_mivs_13)},
        {.data = gal_mivs_14, .len = sizeof(gal_mivs_14)},
        {.data = gal_mivs_15, .len = sizeof(gal_mivs_15)},
        {.data = gal_mivs_16, .len = sizeof(gal_mivs_16)},
        {.data = gal_mivs_17, .len = sizeof(gal_mivs_17)},
        {.data = gal_mivs_18, .len = sizeof(gal_mivs_18)},
        {.data = gal_mivs_19, .len = sizeof(gal_mivs_19)},
    }
};

const galImage_t pillar =
{
    .nFrames = 9,
    .continousPan = NONE,
    .frames = {
        {.data = gal_pillar_00, .len = sizeof(gal_pillar_00)},
        {.data = gal_pillar_01, .len = sizeof(gal_pillar_01)},
        {.data = gal_pillar_02, .len = sizeof(gal_pillar_02)},
        {.data = gal_pillar_03, .len = sizeof(gal_pillar_03)},
        {.data = gal_pillar_04, .len = sizeof(gal_pillar_04)},
        {.data = gal_pillar_05, .len = sizeof(gal_pillar_05)},
        {.data = gal_pillar_06, .len = sizeof(gal_pillar_06)},
        {.data = gal_pillar_07, .len = sizeof(gal_pillar_07)},
        {.data = gal_pillar_08, .len = sizeof(gal_pillar_08)},
    }
};

const galImage_t ragequit =
{
    .nFrames = 46,
    .continousPan = NONE,
    .frames = {
        {.data = gal_ragequit_00, .len = sizeof(gal_ragequit_00)},
        {.data = gal_ragequit_01, .len = sizeof(gal_ragequit_01)},
        {.data = gal_ragequit_02, .len = sizeof(gal_ragequit_02)},
        {.data = gal_ragequit_03, .len = sizeof(gal_ragequit_03)},
        {.data = gal_ragequit_04, .len = sizeof(gal_ragequit_04)},
        {.data = gal_ragequit_05, .len = sizeof(gal_ragequit_05)},
        {.data = gal_ragequit_06, .len = sizeof(gal_ragequit_06)},
        {.data = gal_ragequit_07, .len = sizeof(gal_ragequit_07)},
        {.data = gal_ragequit_08, .len = sizeof(gal_ragequit_08)},
        {.data = gal_ragequit_09, .len = sizeof(gal_ragequit_09)},
        {.data = gal_ragequit_10, .len = sizeof(gal_ragequit_10)},
        {.data = gal_ragequit_11, .len = sizeof(gal_ragequit_11)},
        {.data = gal_ragequit_12, .len = sizeof(gal_ragequit_12)},
        {.data = gal_ragequit_13, .len = sizeof(gal_ragequit_13)},
        {.data = gal_ragequit_14, .len = sizeof(gal_ragequit_14)},
        {.data = gal_ragequit_15, .len = sizeof(gal_ragequit_15)},
        {.data = gal_ragequit_16, .len = sizeof(gal_ragequit_16)},
        {.data = gal_ragequit_17, .len = sizeof(gal_ragequit_17)},
        {.data = gal_ragequit_18, .len = sizeof(gal_ragequit_18)},
        {.data = gal_ragequit_19, .len = sizeof(gal_ragequit_19)},
        {.data = gal_ragequit_20, .len = sizeof(gal_ragequit_20)},
        {.data = gal_ragequit_21, .len = sizeof(gal_ragequit_21)},
        {.data = gal_ragequit_22, .len = sizeof(gal_ragequit_22)},
        {.data = gal_ragequit_23, .len = sizeof(gal_ragequit_23)},
        {.data = gal_ragequit_24, .len = sizeof(gal_ragequit_24)},
        {.data = gal_ragequit_25, .len = sizeof(gal_ragequit_25)},
        {.data = gal_ragequit_26, .len = sizeof(gal_ragequit_26)},
        {.data = gal_ragequit_27, .len = sizeof(gal_ragequit_27)},
        {.data = gal_ragequit_28, .len = sizeof(gal_ragequit_28)},
        {.data = gal_ragequit_29, .len = sizeof(gal_ragequit_29)},
        {.data = gal_ragequit_30, .len = sizeof(gal_ragequit_30)},
        {.data = gal_ragequit_31, .len = sizeof(gal_ragequit_31)},
        {.data = gal_ragequit_32, .len = sizeof(gal_ragequit_32)},
        {.data = gal_ragequit_33, .len = sizeof(gal_ragequit_33)},
        {.data = gal_ragequit_34, .len = sizeof(gal_ragequit_34)},
        {.data = gal_ragequit_35, .len = sizeof(gal_ragequit_35)},
        {.data = gal_ragequit_36, .len = sizeof(gal_ragequit_36)},
        {.data = gal_ragequit_37, .len = sizeof(gal_ragequit_37)},
        {.data = gal_ragequit_38, .len = sizeof(gal_ragequit_38)},
        {.data = gal_ragequit_39, .len = sizeof(gal_ragequit_39)},
        {.data = gal_ragequit_40, .len = sizeof(gal_ragequit_40)},
        {.data = gal_ragequit_41, .len = sizeof(gal_ragequit_41)},
        {.data = gal_ragequit_42, .len = sizeof(gal_ragequit_42)},
        {.data = gal_ragequit_43, .len = sizeof(gal_ragequit_43)},
        {.data = gal_ragequit_44, .len = sizeof(gal_ragequit_44)},
        {.data = gal_ragequit_45, .len = sizeof(gal_ragequit_45)},
    }
};

const galImage_t wink =
{
    .nFrames = 16,
    .continousPan = NONE,
    .frames = {
        {.data = gal_wink_00, .len = sizeof(gal_wink_00)},
        {.data = gal_wink_01, .len = sizeof(gal_wink_01)},
        {.data = gal_wink_02, .len = sizeof(gal_wink_02)},
        {.data = gal_wink_03, .len = sizeof(gal_wink_03)},
        {.data = gal_wink_04, .len = sizeof(gal_wink_04)},
        {.data = gal_wink_05, .len = sizeof(gal_wink_05)},
        {.data = gal_wink_06, .len = sizeof(gal_wink_06)},
        {.data = gal_wink_07, .len = sizeof(gal_wink_07)},
        {.data = gal_wink_08, .len = sizeof(gal_wink_08)},
        {.data = gal_wink_09, .len = sizeof(gal_wink_09)},
        {.data = gal_wink_10, .len = sizeof(gal_wink_10)},
        {.data = gal_wink_11, .len = sizeof(gal_wink_11)},
        {.data = gal_wink_12, .len = sizeof(gal_wink_12)},
        {.data = gal_wink_13, .len = sizeof(gal_wink_13)},
        {.data = gal_wink_14, .len = sizeof(gal_wink_14)},
        {.data = gal_wink_15, .len = sizeof(gal_wink_15)},
    }
};

// Order matters, must match galUnlockPlaceholders
#define NUM_IMAGES 17
const galImage_t* galImages[NUM_IMAGES] =
{
    &galLogo,    // Already unlocked
    &colossus,
    &magvania,
    &allabout,
    &funky,
    &frank,
    &magsquare,
    &josiah,
    &mivs,
    &pillar,
    &bananas_animals,
    &ragequit,
    &wink,
    &galBongo,   // Joust
    &galFunkus,  // Snake
    &galGaylord, // Tiltrads
    &galSnort    // Maze
};

const galImage_t* galUnlockPlaceholders[4] =
{
    &galUnlockJoust,
    &galUnlockSnake,
    &galUnlockTiltrads,
    &galUnlockMaze
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initializer for gallery. Allocate memory, set up timers, and load the first
 * frame of the first image
 */
void ICACHE_FLASH_ATTR galEnterMode(void)
{
    // Clear everything out, for safety
    memset(&gal, 0, sizeof(gal));

    // Allocate a bunch of memory for decompresed images
    gal.compressedData = os_malloc(MAX_DECOMPRESSED_SIZE);
    gal.decompressedData = os_malloc(MAX_DECOMPRESSED_SIZE);
    gal.frameData = os_malloc(MAX_DECOMPRESSED_SIZE);

    // Set up software timers for animation and panning
    os_timer_disarm(&gal.timerAnimate);
    os_timer_setfn(&gal.timerAnimate, (os_timer_func_t*)galLoadNextFrame, NULL);
    os_timer_disarm(&gal.timerPan);
    os_timer_setfn(&gal.timerPan, (os_timer_func_t*)galTimerPan, NULL);

    // Set up a timer to play music
    os_timer_disarm(&gal.timerMusic);
    os_timer_setfn(&gal.timerMusic, (os_timer_func_t*)galTimerMusic, NULL);
    galRearmMusicTimer();

    // Unlock one image by default
    gal.unlockBitmask = getGalleryUnlocks();

    // Load the image
    galLoadFirstFrame();
}

/**
 * Called when gallery is exited. Stop timers and free memory
 */
void ICACHE_FLASH_ATTR galExitMode(void)
{
    // Stop the timers
    os_timer_disarm(&gal.timerAnimate);
    os_timer_disarm(&gal.timerPan);

    // Free the memory
    os_free(gal.compressedData);
    os_free(gal.decompressedData);
    os_free(gal.frameData);
}

/**
 * @brief This rearms the music timer to start playing after 10s
 */
void ICACHE_FLASH_ATTR galRearmMusicTimer(void)
{
    os_timer_disarm(&gal.timerMusic);
    os_timer_arm(&gal.timerMusic, 7 * 1000, false);
}

/**
 * Cycle between the images to display. The flow is to clear the current image,
 * increment to the next image, and load that iamge
 *
 * @param state  A bitmask of all button states, unused
 * @param button The button which triggered this event
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR galButtonCallback(uint8_t state __attribute__((unused)),
        int button __attribute__((unused)), int down __attribute__((unused)))
{
    if(down)
    {
        gal.u = false;
        if(0x8C == (gal.h = ((gal.h << 1) | (button - 1))))
        {
            gal.u |= (unlockGallery(0) | unlockGallery(1) | unlockGallery(2) | unlockGallery(3));
        }

        // Whenever a button is pressed, rearm the music timer
        galRearmMusicTimer();

        switch (button)
        {
            case 2:
            {
                // Right button
                galClearImage();

                // Iterate through the images
                gal.cImage = (gal.cImage + 1) %
                             (sizeof(galImages) / sizeof(galImages[0]));

                // Load it
                galLoadFirstFrame();
                break;
            }
            case 1:
            {
                // Left Button
                galClearImage();

                // Iterate through the images
                if(0 == gal.cImage)
                {
                    gal.cImage = (sizeof(galImages) / sizeof(galImages[0])) - 1;
                }
                else
                {
                    gal.cImage--;
                }

                // Load it
                galLoadFirstFrame();
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
 * @return true  if the image is unlocked
 * @return false if the image is locked
 */
bool ICACHE_FLASH_ATTR galIsImageUnlocked(void)
{
    if(gal.cImage > (NUM_IMAGES - 5))
    {
        // Check to see if it's unlocked
        if(getGalleryUnlocks() & 1 << (gal.cImage - (NUM_IMAGES - 4)))
        {
            // unlocked
            return true;
        }
        else
        {
            // Not unlocked
            return false;
        }
    }
    else
    {
        // First image always unlocked
        return true;
    }
}

/**
 * @return a pointer to the current image to draw, takes into account unlocks
 */
const galImage_t* ICACHE_FLASH_ATTR galGetCurrentImage(void)
{
    const galImage_t* imageToLoad;
    // If we're not on the first image
    if(gal.cImage > (NUM_IMAGES - 5))
    {
        // Check to see if it's unlocked
        if(getGalleryUnlocks() & 1 << (gal.cImage - (NUM_IMAGES - 4)))
        {
            // unlocked
            imageToLoad = galImages[gal.cImage];
        }
        else
        {
            // Not unlocked
            imageToLoad = galUnlockPlaceholders[gal.cImage - (NUM_IMAGES - 4)];
        }
    }
    else
    {
        // First image always unlocked
        imageToLoad = galImages[gal.cImage];
    }
    return imageToLoad;
}

/**
 * For the first frame of an image, load the compressed data from ROM to RAM,
 * decompress the data in RAM, save the metadata, then draw the frame to the
 * OLED
 */
void ICACHE_FLASH_ATTR galLoadFirstFrame(void)
{
    const galImage_t* imageToLoad = galGetCurrentImage();

    /* Read the compressed image from ROM into RAM, and make sure to do a
     * 32 bit aligned read. The arrays are all __attribute__((aligned(4)))
     * so this is safe, not out of bounds
     */
    uint32_t alignedSize = imageToLoad->frames[0].len;
    while(alignedSize % 4 != 0)
    {
        alignedSize++;
    }
    memcpy(gal.compressedData, imageToLoad->frames[0].data, alignedSize);

    // Decompress the image from one RAM area to another
    uint32_t dLen = fastlz_decompress(gal.compressedData,
                                      imageToLoad->frames[0].len,
                                      gal.decompressedData,
                                      MAX_DECOMPRESSED_SIZE);
    DBG_GAL("dLen=%d\n", dLen);

    // Save the metadata
    gal.width      = (gal.decompressedData[0] << 8) | gal.decompressedData[1];
    gal.height     = (gal.decompressedData[2] << 8) | gal.decompressedData[3];
    gal.nFrames    = (gal.decompressedData[4] << 8) | gal.decompressedData[5];
    gal.durationMs = (gal.decompressedData[6] << 8) | gal.decompressedData[7];

    // Save the pan direction
    switch(galImages[gal.cImage]->continousPan)
    {
        case ALWAYS_LEFT:
        {
            gal.virtualWidth = 2 * gal.width;
            gal.panDir = LEFT;
            break;
        }
        case ALWAYS_RIGHT:
        {
            gal.virtualWidth = 2 * gal.width;
            gal.panDir = RIGHT;
            break;
        }
        default:
        case NONE:
        {
            gal.virtualWidth = gal.width;
            break;
        }
    }

    // But never pan the placeholder images
    if(!galIsImageUnlocked())
    {
        gal.virtualWidth = gal.width;
        gal.panDir = NONE;
    }

    DBG_GAL("w=%d, h=%d, nfr=%d, dur=%d repeatw=%d\n", gal.width, gal.height, gal.nFrames,
            gal.durationMs, gal.virtualWidth);

    // Clear gal.frameData, then save the first actual frame
    memset(gal.frameData, 0, MAX_DECOMPRESSED_SIZE);
    memcpy(gal.frameData, &gal.decompressedData[METADATA_LEN], dLen - METADATA_LEN);

    // Set the current frame to 0
    gal.cFrame = 0;

    // Adjust the animation timer to this image's speed
    os_timer_disarm(&gal.timerAnimate);
    if(gal.nFrames > 1)
    {
        os_timer_arm(&gal.timerAnimate, gal.durationMs, true);
    }

    // Set up the panning timer if the image is wider that the OLED
    os_timer_disarm(&gal.timerPan);
    if(gal.virtualWidth > OLED_WIDTH)
    {
        // Pan one pixel every 25 ms for a faster pan
        os_timer_arm(&gal.timerPan, 25, true);
    }

    // Draw the first frame in it's entirety to the OLED
    galDrawFrame();
}

/**
 * For any frame besides the first frame of an image, load the compressed data
 * from ROM to RAM, decompress the data in RAM, modify the current frame with
 * the differences in the decompressed data, then draw the frame to the OLED
 *
 * @param arg Unused
 */
static void ICACHE_FLASH_ATTR galLoadNextFrame(void* arg __attribute__((unused)))
{
    // Increment the current frame
    gal.cFrame = (gal.cFrame + 1) % gal.nFrames;

    // If we're back to the first frame
    if(0 == gal.cFrame)
    {
        // Load and draw the whole frame
        galLoadFirstFrame();
    }
    else
    {
        const galImage_t* imageToLoad = galGetCurrentImage();
        /* Read the compressed image from ROM into RAM, and make sure to do a
        * 32 bit aligned read. The arrays are all __attribute__((aligned(4)))
        * so this is safe, not out of bounds
        */
        uint32_t alignedSize = imageToLoad->frames[gal.cFrame].len;
        while(alignedSize % 4 != 0)
        {
            alignedSize++;
        }
        memcpy(gal.compressedData, imageToLoad->frames[gal.cFrame].data, alignedSize);

        // Decompress the image
        uint32_t dLen = fastlz_decompress(gal.compressedData,
                                          imageToLoad->frames[gal.cFrame].len,
                                          gal.decompressedData,
                                          MAX_DECOMPRESSED_SIZE);
        DBG_GAL("dLen=%d\n", dLen);

        // Adjust only the differences
        for(uint32_t idx = 0; idx < dLen; idx++)
        {
            gal.frameData[idx] ^= gal.decompressedData[idx];
        }

        // Draw the frame
        galDrawFrame();
    }
}

/**
 * Clear all data in RAM from the current image, including the metadata
 * Also stop the timers
 */
void ICACHE_FLASH_ATTR galClearImage(void)
{
    // Stop timers
    os_timer_disarm(&gal.timerAnimate);
    os_timer_disarm(&gal.timerPan);

    // Zero memory, don't free it
    memset(gal.compressedData, 0, MAX_DECOMPRESSED_SIZE);
    memset(gal.decompressedData, 0, MAX_DECOMPRESSED_SIZE);
    memset(gal.frameData, 0, MAX_DECOMPRESSED_SIZE);

    // Clear variables
    gal.width = 0;
    gal.height = 0;
    gal.nFrames = 0;
    gal.cFrame = 0;
    gal.durationMs = 0;
    // Don't reset cImage
    gal.panIdx = 0;
    gal.panDir = RIGHT;
}

/**
 * Draw the current gal.frameData to the OLED, taking into account panning
 */
void ICACHE_FLASH_ATTR galDrawFrame(void)
{
    // Draw the frame to the OLED, one pixel at a time
    int wmod;
    for (int w = 0; w < OLED_WIDTH; w++)
    {
        for (int h = 0; h < OLED_HEIGHT; h++)
        {
            uint16_t linearIdx;
            if(galImages[gal.cImage]->continousPan == NONE)
            {

                linearIdx = (OLED_HEIGHT * (w + gal.panIdx)) + h;
            }
            else
            {
                //At first had this code which blows up if try and pan fast 10ms
                //linearIdx = (OLED_HEIGHT * ((w + gal.panIdx) % gal.width)) + h;
                wmod = w + gal.panIdx;
                while (wmod >= gal.width)
                {
                    wmod -= gal.width;
                }
                linearIdx = OLED_HEIGHT * wmod + h;
            }
            uint16_t byteIdx = linearIdx / 8;
            uint8_t bitIdx = linearIdx % 8;

            if (gal.frameData[byteIdx] & (0x80 >> bitIdx))
            {
                drawPixel(w, h, WHITE);
            }
            else
            {
                drawPixel(w, h, BLACK);
            }
        }
    }

    // Draw left and right arrows to indicate button functions
    fillDisplayArea(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, 4, OLED_HEIGHT, BLACK);
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, "<", TOM_THUMB, WHITE);
    fillDisplayArea(OLED_WIDTH - 4, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB - 1, OLED_WIDTH, OLED_HEIGHT, BLACK);
    plotText(OLED_WIDTH - 3, OLED_HEIGHT - FONT_HEIGHT_TOMTHUMB, ">", TOM_THUMB, WHITE);

    if(gal.u)
    {
        fillDisplayArea(15, 15, OLED_WIDTH - 15, OLED_HEIGHT - 15, BLACK);
        plotRect(16, 16, OLED_WIDTH - 16, OLED_HEIGHT - 16, WHITE);
        plotCenteredText(0, 20, OLED_WIDTH, "Unlock", IBM_VGA_8, WHITE);
        plotCenteredText(0, 20 + FONT_HEIGHT_IBMVGA8 + 4, OLED_WIDTH, "Everything", IBM_VGA_8, WHITE);
    }
}

/**
 * @brief This is called when the music timer expires and starts to play Vivaldi
 *
 * @param arg unused
 */
static void ICACHE_FLASH_ATTR galTimerMusic(void* arg __attribute__((unused)))
{
    static bool isPlaying = false;
    if(false == isPlaying)
    {
        startBuzzerSong(getSong(gal.cImage));
        isPlaying = true;
    }
}

/**
 * Timer function to pan the image left and right, if it is wider than the OLED
 *
 * @param arg Unused
 */
static void ICACHE_FLASH_ATTR galTimerPan(void* arg __attribute__((unused)))
{
    if(gal.virtualWidth > OLED_WIDTH)
    {
        switch(gal.panDir)
        {
            case RIGHT:
            {
                // If we're at the end
                if((gal.virtualWidth - OLED_WIDTH) == gal.panIdx)
                {
                    if (galImages[gal.cImage]->continousPan == ALWAYS_RIGHT)
                    {
                        // reset for pan
                        gal.panIdx = (gal.virtualWidth - OLED_WIDTH) - gal.width;
                    }
                    else
                    {
                        // Start going to the left
                        gal.panDir = LEFT;
                    }
                }
                else
                {
                    // Pan to the right
                    gal.panIdx++;
                }
                break;
            }
            case LEFT:
            {
                // If we're at the beginning
                if(0 == gal.panIdx)
                {
                    if (galImages[gal.cImage]->continousPan == ALWAYS_LEFT)
                    {
                        // reset for pan
                        gal.panIdx = gal.width;
                    }
                    else
                    {
                        // Start going to the right
                        gal.panDir = RIGHT;
                    }
                }
                else
                {
                    // Pan to the left (objects seem to move from L to R)
                    gal.panIdx--;
                }
                break;
            }
            default:
            {
                break;
            }
        }
        // Draw the panned frame
        galDrawFrame();
    }
}
