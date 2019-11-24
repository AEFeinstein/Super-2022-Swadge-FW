/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <spi_flash.h>
#include <gpio.h>
#include <eagle_soc.h>

#include "ccconfig.h"
#include "DFT32.h"
#include "embeddednf.h"
#include "embeddedout.h"
#include "custom_commands.h"
#include "user_main.h"
#include "spi_memory_addrs.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define NUMBER_STORED_CONFIGURABLES 16
#define CONFIGURABLES sizeof(struct CCSettings) //(plus1)
#define SAVE_LOAD_KEY 0xB0

/*============================================================================
 * Structs
 *==========================================================================*/

// Should be no larger than USER_SETTINGS_SIZE
typedef struct __attribute__((aligned(4)))
{
    uint8_t SaveLoadKey; //Must be SAVE_LOAD_KEY to be valid.
    uint8_t configs[CONFIGURABLES];
    uint32_t ttHighScores[NUM_TT_HIGH_SCORES]; //first,second,third
    uint32_t ttLastScore;
    uint32_t mzBestTimes[NUM_MZ_LEVELS]; //best for each level
    uint32_t mzLastScore;
    uint32_t joustWins;
    uint32_t snakeHighScores[3];
    uint32_t galleryUnlocks;
    bool isMuted;
}
settings_t;

typedef struct
{
    uint8_t defaultVal;
    char* name;
    uint8_t* val;
} configurable_t;

/*============================================================================
 * Variables
 *==========================================================================*/

#if PROGRESSIVE_DFT == 0
    #define DUP 128
#else
    #define DUP 1
#endif

uint8_t gConfigDefaults[NUMBER_STORED_CONFIGURABLES][CONFIGURABLES] = {
//   i  r o d d   f     f l  s j  c    a d m    a d m    m  m b   s     p  u                       s  a d    s  f    d      s w   c  n                 e
//   a  m f i u   i     b c  b m  o    1 1 1    2 2 2    a  a r   a     r  s                       y  c r    h  l    i      r r   f  l                 q
//      s f r p   r     p o  b p  m                      n  d i   t     o  e                       m  t v    f  p    s      t p   #  d                 u
	{32,1,0,3,DUP,1,    5,15,0,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS,       0, 1,0,   0, 0,   0,     1,1,  0, DEFAULT_NUM_LEDS, 0, 0},
	{16,1,0,6,DUP,3,    5,14,3,85,0,   0,3,208, 0,3,102, 45,8,100,255,  55,DEFAULT_NUM_LEDS-2,     0, 1,8,   4, 32 , 1,     0,0,  1, DEFAULT_NUM_LEDS, 0, 0},
	{16,1,0,5,DUP,3,    5,14,3,85,0,   4,4,152, 7,7,145, 82,1,255,255,  15,DEFAULT_NUM_LEDS/3,     0, 1,0,   1, 1,   2,     0,0,  2, DEFAULT_NUM_LEDS, 0, 0},
	{ 8,0,0,6,DUP,6,    5,21,3,42,0,   2,2,124, 4,4,16,  45,1,100,255,  10,DEFAULT_NUM_LEDS,       0, 1,254, 20,0,   0,     0,0,  3, DEFAULT_NUM_LEDS, 0, 0},
	{16,0,0,2,DUP,1,    5,14,3,85,0,   0,4,32,  0,6,16,  82,1,100,255,  55,DEFAULT_NUM_LEDS/3-2,   2, 1,4, 100, 0, 255,     0,0,  4, DEFAULT_NUM_LEDS, 0, 0},
	{16,0,0,2,DUP,1,    5,14,3,85,0,   0,4,32,  0,6,16,  82,1,100,255,  55,DEFAULT_NUM_LEDS/3-2,   2, 1,10,  0, 0,   0,     0,0,  5, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS,       0, 1,10,  0, 0,   0,     0,0,  6, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS,       0, 1,11,  0, 0,   0,     0,0,  7, DEFAULT_NUM_LEDS, 0, 0},
	{16,0,0,3,DUP,2,    0,11,3,42,0,   2,2,16,  4,4,81,  45,1,100,255,  15,DEFAULT_NUM_LEDS,       0, 1,255, 0, 0,   0,     0,0,  8, DEFAULT_NUM_LEDS, 0, 0},
	{16,0,0,6,DUP,3,    5,14,3,255,0,  3,1,180, 8,7,187, 45,4,100,255,  55,DEFAULT_NUM_LEDS-2,     0, 1,8,   0, 1,   1,     0,0,  9, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS/2,     1, 1,0,   0, 0,   0,     1,1, 10, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS/3,     2, 1,0,   0, 0,   0,     1,1, 11, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS/4,     3, 1,0,   0, 0,   0,     1,1, 12, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS/2,     0, 1,0,  65,65,   0,     1,1, 13, DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,6,69,  45,1,100,255, 103,DEFAULT_NUM_LEDS,       0, 1,1,   0, 0,   0,     1,1,  14,DEFAULT_NUM_LEDS, 0, 0},
	{32,1,0,6,DUP,3,    5,15,3,44,0,   2,2,96,  4,4,69,  45,1,100,255, 101,DEFAULT_NUM_LEDS/8,     0, 1,0,   1, 31,  1,     1,0,  15,DEFAULT_NUM_LEDS, 0, 0},
	};


uint8_t* gConfigurables[CONFIGURABLES] = { &CCS.gINITIAL_AMP, &CCS.gRMUXSHIFT,  &CCS.gROOT_NOTE_OFFSET, &CCS.gDFTIIR, &CCS.gDFT_UPDATE, &CCS.gFUZZ_IIR_BITS,
                                           &CCS.gFILTER_BLUR_PASSES, &CCS.gLOWER_CUTOFF, &CCS.gSEMIBITSPERBIN, &CCS.gMAX_JUMP_DISTANCE, &CCS.gMAX_COMBINE_DISTANCE,
                                           &CCS.gAMP1_ATTACK_BITS, &CCS.gAMP1_DECAY_BITS, &CCS.gAMP_1_MULT, &CCS.gAMP2_ATTACK_BITS, &CCS.gAMP2_DECAY_BITS, &CCS.gAMP_2_MULT, &CCS.gMIN_AMP_FOR_NOTE,
                                           &CCS.gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR, &CCS.gNOTE_FINAL_AMP, &CCS.gNOTE_FINAL_SATURATION,
                                           &CCS.gNERF_NOTE_PORP, &CCS.gUSE_NUM_LIN_LEDS, &CCS.gSYMMETRY_REPEAT, &CCS.gCOLORCHORD_ACTIVE, &CCS.gCOLORCHORD_OUTPUT_DRIVER, &CCS.gCOLORCHORD_SHIFT_INTERVAL,
                                           &CCS.gCOLORCHORD_FLIP_ON_PEAK, &CCS.gCOLORCHORD_SHIFT_DISTANCE, &CCS.gCOLORCHORD_SORT_NOTES, &CCS.gCOLORCHORD_LIN_WRAPAROUND, &CCS.gCONFIG_NUMBER,
                                           &CCS.gNUM_LIN_LEDS, &CCS.gEQUALIZER_SET,  0
                                         };

char* gConfigurableNames[CONFIGURABLES] = { "gINITIAL_AMP", "gRMUXSHIFT", "gROOT_NOTE_OFFSET", "gDFTIIR", "gDFT_UPDATE", "gFUZZ_IIR_BITS",
                                            "gFILTER_BLUR_PASSES", "gLOWER_CUTOFF", "gSEMIBITSPERBIN", "gMAX_JUMP_DISTANCE", "gMAX_COMBINE_DISTANCE", "gAMP1_ATTACK_BITS", "gAMP1_DECAY_BITS",
                                            "gAMP_1_MULT", "gAMP2_ATTACK_BITS", "gAMP2_DECAY_BITS", "gAMP_2_MULT", "gMIN_AMP_FOR_NOTE", "gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR", "gNOTE_FINAL_AMP",
                                            "gNOTE_FINAL_SATURATION", "gNERF_NOTE_PORP", "gUSE_NUM_LIN_LEDS", "gSYMMETRY_REPEAT", "gCOLORCHORD_ACTIVE", "gCOLORCHORD_OUTPUT_DRIVER",
                                            "gCOLORCHORD_SHIFT_INTERVAL", "gCOLORCHORD_FLIP_ON_PEAK", "gCOLORCHORD_SHIFT_DISTANCE", "gCOLORCHORD_SORT_NOTES", "gCOLORCHORD_LIN_WRAPAROUND",
                                            "gCONFIG_NUMBER", "gNUM_LIN_LEDS",  "gEQUALIZER_SET", 0
                                          };

struct CCSettings CCS = {0};

configurable_t gConfigs[CONFIGURABLES] = {{0}};
// {
//     {
//         .defaultVal = 0,
//         .name = 0,
//         .val = 0
//     }
// };

settings_t settings =
{
    .SaveLoadKey = 0,
    .configs = {0},
    .joustWins = 0,
    .snakeHighScores = {0},
    .ttHighScores = {0},
    .ttLastScore = 0,
    .mzBestTimes = {0},
    .mzLastScore = 0,
};

bool muteOverride = false;

/*============================================================================
 * Prototypes
 *==========================================================================*/

void ICACHE_FLASH_ATTR SaveSettings(void);
//void ICACHE_FLASH_ATTR RevertAndSaveAllSettingsExceptLEDs(void);

/*============================================================================
 * Functions
 *==========================================================================*/

void ICACHE_FLASH_ATTR PopulategConfigs(void)
{
    for( uint8_t i = 0; i < CONFIGURABLES - 1; i++ )
    {
        gConfigs[i].defaultVal = gConfigDefaults[0][i];
        gConfigs[i].name = gConfigurableNames[i];
        gConfigs[i].val = gConfigurables[i];
        os_printf("i %d, defaultVal %d, name %s, val %x\n", i, gConfigs[i].defaultVal, gConfigs[i].name, (uint32_t)gConfigs[i].val);
        //os_printf("i %d, defaultVal %d, name %s\n", i, gConfigs[i].defaultVal, gConfigs[i].name);
        //os_printf("i %d\n", i);
    }
}

/**
 * Initialization for settings, called by user_init().
 * Reads settings from SPI flash into gConfigs.
 * This will load defaults if a key value isn't present in SPI flash.
 */
void ICACHE_FLASH_ATTR LoadSettings(void)
{
    spi_flash_read( USER_SETTINGS_ADDR, (uint32*)&settings, sizeof( settings ) );
    if( settings.SaveLoadKey == SAVE_LOAD_KEY )
    {
        os_printf("Settings found\r\n");
        // load gConfigs from the settings
        for( uint8_t i = 0; i < CONFIGURABLES; i++ )
        {
            if( gConfigs[i].val )
            {
                *(gConfigs[i].val) = settings.configs[i];
            }
        }
    }
    else
    {
        os_printf("Settings not found\r\n");
        // Zero everything
        memset(&settings, 0, sizeof(settings));
        // Set the key
        settings.SaveLoadKey = SAVE_LOAD_KEY;
        // Load in default values
        for(uint8_t i = 0; i < CONFIGURABLES; i++ )
        {
            if( gConfigs[i].val )
            {
                *(gConfigs[i].val) = gConfigs[i].defaultVal;
            }
        }
        memset(settings.ttHighScores, 0, NUM_TT_HIGH_SCORES * sizeof(uint32_t));
        memset(settings.mzBestTimes, 0x0f, NUM_MZ_LEVELS * sizeof(uint32_t));
        settings.mzLastScore = 100000;
        settings.joustWins = 0;
        settings.galleryUnlocks = 0;
        SaveSettings(); // updates settings.configs then saves
    }
}

/**
 * Save all settings from gConfigs[] to SPI flash
 */
void ICACHE_FLASH_ATTR SaveSettings(void)
{
    uint8_t i;
    for( i = 0; i < CONFIGURABLES; i++ )
    {
        if( gConfigs[i].val )
        {
            settings.configs[i] = *(gConfigs[i].val);
        }
    }

    EnterCritical();
    spi_flash_erase_sector( USER_SETTINGS_ADDR / SPI_FLASH_SEC_SIZE );
    spi_flash_write( USER_SETTINGS_ADDR, (uint32*)&settings, ((sizeof( settings ) - 1) & (~0xf)) + 0x10 );
    ExitCritical();
}

/**
 * @brief Save a high score from the snake game
 *
 * @param difficulty The 0-indexed difficulty (easy, medium, hard)
 * @param score      The score to save
 */
void ICACHE_FLASH_ATTR setSnakeHighScore(uint8_t difficulty, uint32_t score)
{
    if(score > settings.snakeHighScores[difficulty])
    {
        settings.snakeHighScores[difficulty] = score;
        SaveSettings();
    }
}


/**
 * Increment the game win count and save it to SPI flash
 */
void ICACHE_FLASH_ATTR setJoustWins(uint32_t elo)
{
    settings.joustWins = elo;
    SaveSettings();
}


/**
 * @return A pointer to the three Snake high scores
 */
uint32_t* ICACHE_FLASH_ATTR getSnakeHighScores(void)
{
    // Loaded on boot
    return settings.snakeHighScores;
}

/**
 * @return The number of reflector games this swadge has won
 */
uint32_t ICACHE_FLASH_ATTR getJoustWins(void)
{
    return settings.joustWins;
}

uint32_t* ICACHE_FLASH_ATTR ttGetHighScores(void)
{
    return settings.ttHighScores;
}

void ICACHE_FLASH_ATTR ttSetHighScores(uint32_t* newHighScores)
{
    memcpy(settings.ttHighScores, newHighScores, NUM_TT_HIGH_SCORES * sizeof(uint32_t));
    SaveSettings();
}

uint32_t ICACHE_FLASH_ATTR ttGetLastScore(void)
{
    return settings.ttLastScore;
}

void ICACHE_FLASH_ATTR ttSetLastScore(uint32_t newLastScore)
{
    settings.ttLastScore = newLastScore;
    SaveSettings();
}

uint32_t* ICACHE_FLASH_ATTR mzGetBestTimes(void)
{
    return settings.mzBestTimes;
}

void ICACHE_FLASH_ATTR mzSetBestTimes(uint32_t* newHighScores)
{
    memcpy(settings.mzBestTimes, newHighScores, NUM_MZ_LEVELS * sizeof(uint32_t));
    SaveSettings();
}

uint32_t ICACHE_FLASH_ATTR mzGetLastScore(void)
{
    return settings.mzLastScore;
}

void ICACHE_FLASH_ATTR mzSetLastScore(uint32_t newLastScore)
{
    settings.mzLastScore = newLastScore;
    SaveSettings();
}

/**
 * @brief 
 * 
 * @param opt true to keep sound on, false to use the flash muted option 
 */
void ICACHE_FLASH_ATTR setMuteOverride(bool opt)
{
    muteOverride = opt;
}

bool ICACHE_FLASH_ATTR getIsMutedOption(void)
{
    //os_printf("%d %d %d\n", muteOverride, settings.isMuted, settings.isMuted && !muteOverride);
    return settings.isMuted && !muteOverride;
}

void ICACHE_FLASH_ATTR setIsMutedOption(bool mute)
{
    settings.isMuted = mute;
    SaveSettings();
}

/**
 * @return A bitmask of unlocked images
 */
uint32_t ICACHE_FLASH_ATTR getGalleryUnlocks(void)
{
    return 0xffffffff;
    //return settings.galleryUnlocks;
}

/**
 * @brief Set a bit at the given index in the unlocked images bitmask
 * 
 * @param idx 0 - Bongos for Joust
 *            1 - Funkus for Snake
 *            2 - Gaylord for Tiltrads
 *            3 - Snortmelon for Maze
 * @return true  if the bit was just set
 * @return false if the bit was already set
 */
bool ICACHE_FLASH_ATTR unlockGallery(uint8_t idx)
{
    if(!(settings.galleryUnlocks & (1 << idx)))
    {
        settings.galleryUnlocks |= (1 << idx);
        SaveSettings();
        return true;
    }
    return false;
}

/**
 * Revert all settings to their default values, except gUSE_NUM_LIN_LEDS
 * Once the settings are reverted, except for gUSE_NUM_LIN_LEDS, write
 * the settings to SPI flash
 */
//void ICACHE_FLASH_ATTR RevertAndSaveAllSettingsExceptLEDs(void)
//{
//    os_printf( "Restoring all values.\n" );
//
//    // Save gUSE_NUM_LIN_LEDS
//    int led = CCS.gUSE_NUM_LIN_LEDS;
//    if( led == 0 )
//    {
//        led = 5;
//    }
//
//    // Restore to defaults
//    uint8_t i;
//    for( i = 0; i < CONFIGURABLES; i++ )
//    {
//        if( gConfigs[i].val )
//        {
//            *(gConfigs[i].val) = gConfigs[i].defaultVal;
//        }
//    }
//
//    // Restore saved gUSE_NUM_LIN_LEDS
//    CCS.gUSE_NUM_LIN_LEDS = led;
//
//    // Write to SPI flash
//    SaveSettings();
//}
