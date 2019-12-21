/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <spi_flash.h>
#include <gpio.h>
#include <eagle_soc.h>

#include "ccconfig.h"
#include "DFT32.h"
#include "embeddedout.h"
#include "custom_commands.h"
#include "user_main.h"
#include "spi_memory_addrs.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define SAVE_LOAD_KEY 0xB4

/*============================================================================
 * Structs
 *==========================================================================*/

// Should be no larger than USER_SETTINGS_SIZE
typedef struct __attribute__((aligned(4)))
{
    uint8_t SaveLoadKey; //Must be SAVE_LOAD_KEY to be valid.
    uint32_t ttHighScores[NUM_TT_HIGH_SCORES]; //first,second,third
    uint32_t ttLastScore;
    uint32_t mzBestTimes[NUM_MZ_LEVELS]; //best for each level
    uint32_t mzLastScore;
    uint32_t joustWins;
    uint32_t snakeHighScores[3];
    uint32_t galleryUnlocks;
    bool isMuted;
    uint8_t mazeLevel;
    uint8_t menuPos;
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

settings_t settings =
{
    .SaveLoadKey = 0,
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
    }
    else
    {
        os_printf("Settings not found\r\n");
        // Zero everything
        memset(&settings, 0, sizeof(settings));
        // Set the key
        settings.SaveLoadKey = SAVE_LOAD_KEY;
        // Load in default values
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

uint8_t ICACHE_FLASH_ATTR getMazeLevel(void)
{
    return settings.mazeLevel;
}

void ICACHE_FLASH_ATTR setMazeLevel(uint8_t level)
{
    settings.mazeLevel = level;
    SaveSettings();
}

uint8_t ICACHE_FLASH_ATTR getMenuPos(void)
{
    return settings.menuPos;
}

void ICACHE_FLASH_ATTR setMenuPos(uint8_t pos)
{
    settings.menuPos = pos;
    SaveSettings();
}

/**
 * @return A bitmask of unlocked images
 */
uint32_t ICACHE_FLASH_ATTR getGalleryUnlocks(void)
{
    return settings.galleryUnlocks;
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
