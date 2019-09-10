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

#define CONFIGURABLES sizeof(struct CCSettings) //(plus1)
#define SAVE_LOAD_KEY 0xAA

/*============================================================================
 * Structs
 *==========================================================================*/

// Should be no larger than USER_SETTINGS_SIZE
typedef struct __attribute__((aligned(4)))
{
    uint8_t SaveLoadKey; //Must be SAVE_LOAD_KEY to be valid.
    uint8_t configs[CONFIGURABLES];
    uint8_t refGameWins;
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

struct CCSettings CCS = {0};

configurable_t gConfigs[CONFIGURABLES] =
{
    {
        .defaultVal = 0,
        .name = "gROOT_NOTE_OFFSET",
        .val = &CCS.gROOT_NOTE_OFFSET
    },
    {
        .defaultVal = 6,
        .name = "gDFTIIR",
        .val = &CCS.gDFTIIR
    },
    {
        .defaultVal = 1,
        .name = "gFUZZ_IIR_BITS",
        .val = &CCS.gFUZZ_IIR_BITS
    },
    {
        .defaultVal = 2,
        .name = "gFILTER_BLUR_PASSES",
        .val = &CCS.gFILTER_BLUR_PASSES
    },
    {
        .defaultVal = 3,
        .name = "gSEMIBITSPERBIN",
        .val = &CCS.gSEMIBITSPERBIN
    },
    {
        .defaultVal = 4,
        .name = "gMAX_JUMP_DISTANCE",
        .val = &CCS.gMAX_JUMP_DISTANCE
    },
    {
        .defaultVal = 7,
        .name = "gMAX_COMBINE_DISTANCE",
        .val = &CCS.gMAX_COMBINE_DISTANCE
    },
    {
        .defaultVal = 4,
        .name = "gAMP_1_IIR_BITS",
        .val = &CCS.gAMP_1_IIR_BITS
    },
    {
        .defaultVal = 2,
        .name = "gAMP_2_IIR_BITS",
        .val = &CCS.gAMP_2_IIR_BITS
    },
    {
        .defaultVal = 80,
        .name = "gMIN_AMP_FOR_NOTE",
        .val = &CCS.gMIN_AMP_FOR_NOTE
    },
    {
        .defaultVal = 64,
        .name = "gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR",
        .val = &CCS.gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR
    },
    {
        .defaultVal = 12,
        .name = "gNOTE_FINAL_AMP",
        .val = &CCS.gNOTE_FINAL_AMP
    },
    {
        .defaultVal = 15,
        .name = "gNERF_NOTE_PORP",
        .val = &CCS.gNERF_NOTE_PORP
    },
    {
        .defaultVal = NUM_LIN_LEDS,
        .name = "gUSE_NUM_LIN_LEDS",
        .val = &CCS.gUSE_NUM_LIN_LEDS
    },
    {
        .defaultVal = 1,
        .name = "gCOLORCHORD_ACTIVE",
        .val = &CCS.gCOLORCHORD_ACTIVE
    },
    {
        .defaultVal = 1,
        .name = "gCOLORCHORD_OUTPUT_DRIVER",
        .val = &CCS.gCOLORCHORD_OUTPUT_DRIVER
    },
    {
        .defaultVal = 20,
        .name = "gINITIAL_AMP",
        .val = &CCS.gINITIAL_AMP
    },
    {
        .defaultVal = 0,
        .name = 0,
        .val = 0
    }
};

uint8_t refGameWins = 0;

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
    settings_t settings =
    {
        .SaveLoadKey = 0,
        .configs = {0},
        .refGameWins = 0
    };

    uint8_t i;
    spi_flash_read( USER_SETTINGS_ADDR, (uint32*)&settings, sizeof( settings ) );
    if( settings.SaveLoadKey == SAVE_LOAD_KEY )
    {
        os_printf("Settings found\r\n");
        for( i = 0; i < CONFIGURABLES; i++ )
        {
            if( gConfigs[i].val )
            {
                *gConfigs[i].val = settings.configs[i];
            }
        }

        refGameWins = settings.refGameWins;
    }
    else
    {
        os_printf("Settings not found\r\n");
        for( i = 0; i < CONFIGURABLES; i++ )
        {
            if( gConfigs[i].val )
            {
                *gConfigs[i].val = gConfigs[i].defaultVal;
            }
        }
        refGameWins = 0;
        SaveSettings();
    }
}

/**
 * Save all settings from gConfigs[] to SPI flash
 */
void ICACHE_FLASH_ATTR SaveSettings(void)
{
    settings_t settings =
    {
        .SaveLoadKey = SAVE_LOAD_KEY,
        .configs = {0},
        .refGameWins = refGameWins
    };

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
 * Increment the game win count and save it to SPI flash
 */
void ICACHE_FLASH_ATTR incrementRefGameWins(void)
{
    if(refGameWins != 0xFF)
    {
        refGameWins++;
        SaveSettings();
    }
}

/**
 * Set the game wins to max, unlocking all patterns
 */
void ICACHE_FLASH_ATTR setGameWinsToMax(void)
{
    if(refGameWins != 0xFF)
    {
        refGameWins = 0xFF;
        SaveSettings();
    }
}

/**
 * @return The number of reflector games this swadge has won
 */
uint8_t ICACHE_FLASH_ATTR getRefGameWins(void)
{
    return refGameWins;
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
