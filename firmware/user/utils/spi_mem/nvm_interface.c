/*============================================================================
 * Includes
 *==========================================================================*/

#include <osapi.h>
#include <spi_flash.h>
#include <gpio.h>
#include <eagle_soc.h>

#include "esp_niceness.h"
#include "hsv_utils.h"
#include "nvm_interface.h"
#include "user_main.h"
#include "printControl.h"

/*============================================================================
 * Defines
 *==========================================================================*/

#define SAVE_LOAD_KEY 0xC5

/*============================================================================
 * Structs
 *==========================================================================*/

// Should be no larger than USER_SETTINGS_SIZE
typedef struct __attribute__((aligned(4)))
{
    uint8_t SaveLoadKey; //Must be SAVE_LOAD_KEY to be valid.
    bool isMuted;
    uint8_t menuPos;
    demon_t savedDemon;
    ddrHighScores_t ddrHighScores;
    demonMemorial_t demonMemorials[NUM_DEMON_MEMORIALS];
    flightSimSaveData_t flightSaveData;
    char gitHash[32];
    bool selfTestPassed;
    raycasterScores_t raycasterScores;
    char ssid[SSID_NAME_LEN];
    char ssidPw[SSID_NAME_LEN];
    mtHighScores_t mtHighScores;
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

settings_t settings = {0};
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
        INIT_PRINTF("Settings found\r\n");
    }
    else
    {
        INIT_PRINTF("Settings not found\r\n");
        // Zero everything
        ets_memset(&settings, 0, sizeof(settings));
        // Set the key
        settings.SaveLoadKey = SAVE_LOAD_KEY;
        // Load in default values
        settings.isMuted = false;
        ets_memset(&(settings.savedDemon), 0, sizeof(demon_t));
        settings.selfTestPassed = false;
        
        for(int i = 0; i < NUM_DEMON_MEMORIALS; i++)
        {
            settings.demonMemorials[i].actionsTaken = -1;
        }
        // Save the values
        SaveSettings();
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

uint8_t ICACHE_FLASH_ATTR getMenuPos(void)
{
    return settings.menuPos;
}

void ICACHE_FLASH_ATTR setMenuPos(uint8_t pos)
{
    settings.menuPos = pos;
    SaveSettings();
}

#if defined(FEATURE_BZR)
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
    return settings.isMuted && !muteOverride;
}

void ICACHE_FLASH_ATTR setIsMutedOption(bool mute)
{
    settings.isMuted = mute;
    SaveSettings();
}
#endif

void ICACHE_FLASH_ATTR getSavedDemon(demon_t* demon)
{
    ets_memcpy(demon, &(settings.savedDemon), sizeof(demon_t));
}

void ICACHE_FLASH_ATTR setSavedDemon(demon_t* demon)
{
    ets_memcpy(&(settings.savedDemon), demon, sizeof(demon_t));
    SaveSettings();
}

void ICACHE_FLASH_ATTR getDDRScores(ddrHighScores_t* highScores)
{
    ets_memcpy(highScores, &(settings.ddrHighScores), sizeof(ddrHighScores_t));
}

void ICACHE_FLASH_ATTR setDDRScores(ddrHighScores_t* highScores)
{
    ets_memcpy(&(settings.ddrHighScores), highScores, sizeof(ddrHighScores_t));
    SaveSettings();
}

void ICACHE_FLASH_ATTR getMTScores(mtHighScores_t* highScores)
{
    ets_memcpy(highScores, &(settings.mtHighScores), sizeof(mtHighScores_t));
}

void ICACHE_FLASH_ATTR setMTScores(mtHighScores_t* highScores)
{
    ets_memcpy(&(settings.mtHighScores), highScores, sizeof(mtHighScores_t));
    SaveSettings();
}

void ICACHE_FLASH_ATTR addDemonMemorial(char* name, int32_t actionsTaken)
{
    // Keep the memorials in high to low order. Find the insertion index
    uint8_t insertionIdx = 0;
    for(insertionIdx = 0; insertionIdx < NUM_DEMON_MEMORIALS; insertionIdx++)
    {
        if(actionsTaken > settings.demonMemorials[insertionIdx].actionsTaken)
        {
            break;
        }
    }

    // If this would be inserted after the list, just return
    if(insertionIdx == NUM_DEMON_MEMORIALS)
    {
        return;
    }

    // If this would be inserted anywhere except the end of the list
    if(insertionIdx != (NUM_DEMON_MEMORIALS - 1))
    {
        // Move all the memory after this index to make room for insertion
        ets_memmove(
            &settings.demonMemorials[insertionIdx + 1],
            &settings.demonMemorials[insertionIdx],
            (NUM_DEMON_MEMORIALS - insertionIdx - 1) * sizeof(demonMemorial_t));
    }

    // Copy in the name and actions taken
    ets_memcpy(&(settings.demonMemorials[insertionIdx].name), name, ets_strlen(name) + 1);
    settings.demonMemorials[insertionIdx].actionsTaken = actionsTaken;

    // Save the settings
    SaveSettings();
}

demonMemorial_t* ICACHE_FLASH_ATTR getDemonMemorials(void)
{
    return settings.demonMemorials;
}

/**
 * Copy the git hash string from NVM to a pointer
 *
 * @param hash A pointer to copy the git hash to. MUST BE AT LEAST 32 BYTES LONG
 */
void ICACHE_FLASH_ATTR getGitHash(char* hash)
{
    ets_memcpy(hash, &(settings.gitHash), ets_strlen(settings.gitHash) + 1);
}

/**
 * Write the git has string to NVM, clipping it to 31 chars at most
 *
 * @param hash A pointer to the git hash to write
 */
void ICACHE_FLASH_ATTR setGitHash(char* hash)
{
    // Make sure we save a valid string by clipping it if it's too long
    uint16_t len = ets_strlen(hash);
    if(len > sizeof(settings.gitHash) - 1)
    {
        len = sizeof(settings.gitHash) - 1;
        hash[sizeof(settings.gitHash) - 1] = 0;
    }
    ets_memcpy(&(settings.gitHash), hash, len + 1);
    SaveSettings();
}

bool ICACHE_FLASH_ATTR getSelfTestPass(void)
{
    return settings.selfTestPassed;
}

void ICACHE_FLASH_ATTR setSelfTestPass(bool pass)
{
    settings.selfTestPassed = pass;
    SaveSettings();
}


raycasterScores_t* ICACHE_FLASH_ATTR getRaycasterScores(void)
{
    return &(settings.raycasterScores);
}

void ICACHE_FLASH_ATTR addRaycasterScore(raycasterDifficulty_t difficulty, raycasterMap_t mapIdx, uint16_t kills,
        uint32_t tElapsed)
{
    // Look through the table for a spot tto insert
    for(uint8_t i = 0; i < RC_NUM_SCORES; i++)
    {
        // More kills, insert before this index
        if((settings.raycasterScores.scores[mapIdx][difficulty][i].kills < kills) ||
                // Same kills but faster, insert before this index
                (settings.raycasterScores.scores[mapIdx][difficulty][i].kills == kills &&
                 settings.raycasterScores.scores[mapIdx][difficulty][i].tElapsedUs > tElapsed) ||
                // Or slot is empty, insert before this index
                (settings.raycasterScores.scores[mapIdx][difficulty][i].kills == 0 &&
                 settings.raycasterScores.scores[mapIdx][difficulty][i].tElapsedUs == 0))
        {
            // Make room for this element
            ets_memmove(&settings.raycasterScores.scores[mapIdx][difficulty][i + 1],
                        &settings.raycasterScores.scores[mapIdx][difficulty][i],
                        sizeof(raycasterScore_t ) * (RC_NUM_SCORES - 1 - i));

            // Write the new data
            settings.raycasterScores.scores[mapIdx][difficulty][i].kills = kills;
            settings.raycasterScores.scores[mapIdx][difficulty][i].tElapsedUs = tElapsed;

            // Save the settings
            SaveSettings();

            return;
        }
    }
}

flightSimSaveData_t* ICACHE_FLASH_ATTR getFlightSaveData(void)
{
    return &settings.flightSaveData;
}

void ICACHE_FLASH_ATTR setFlightSaveData( flightSimSaveData_t* t )
{
    if( t != &settings.flightSaveData )
    {
        memcpy( t, &settings.flightSaveData, sizeof( *t ) );
    }
    SaveSettings();
}

void ICACHE_FLASH_ATTR getSsidPw(char* ssid, char* pw)
{
    memcpy(ssid, settings.ssid, SSID_NAME_LEN);
    memcpy(pw, settings.ssidPw, SSID_NAME_LEN);
}

void ICACHE_FLASH_ATTR setSsidPw(char* ssid, char* pw)
{
    memcpy(settings.ssid, ssid, SSID_NAME_LEN);
    memcpy(settings.ssidPw, pw, SSID_NAME_LEN);
    SaveSettings();
}

void ICACHE_FLASH_ATTR clearAllNvm(void)
{
    ets_memset(&settings, 0, sizeof(settings));
    SaveSettings();
}