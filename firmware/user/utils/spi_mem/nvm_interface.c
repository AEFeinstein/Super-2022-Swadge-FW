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

settings_t settings =
{
    .SaveLoadKey = 0,
    .isMuted = FALSE
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
        ets_memset(&settings, 0, sizeof(settings));
        // Set the key
        settings.SaveLoadKey = SAVE_LOAD_KEY;
        // Load in default values
        settings.isMuted = false;
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
