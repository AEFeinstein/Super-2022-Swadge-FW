So you want to save some data? Maybe it's some sort of progress tracker, maybe it's a set of high scores. Lets walk through how to do that.

The file that manages persistent data is `firmware/user/utils/spi_mem/nvm_interface.c`. The `settings_t` struct provides a map for where data is saved in nonvolatile memory (NVM), and the `settings` varible acts as a copy of NVM in RAM.

To add data to NVM, follow these steps. In this example we'll add a `bool` called `selfTestPassed`.

1. Add space for the data you want to save in the `settings_t`.
    ```
    // Should be no larger than USER_SETTINGS_SIZE
    typedef struct __attribute__((aligned(4)))
    {
        uint8_t SaveLoadKey; //Must be SAVE_LOAD_KEY to be valid.
        bool isMuted;
        uint8_t menuPos;
        demon_t savedDemon;
        ddrHighScores_t ddrHighScores;
        demonMemorial_t demonMemorials[NUM_DEMON_MEMORIALS];
        char gitHash[32];
        bool selfTestPassed;
    }
    settings_t;
    ```
1. Increment the `SAVE_LOAD_KEY` #define. This is kind of like a version for the memory layout. If it changes, the firmware knows to reinitialize NVM
    ```
    #define SAVE_LOAD_KEY 0xBD
    ```
1. Set an initial value for your data in the RAM copy of the NVM data, `settings`
    ```
    settings_t settings =
    {
        .SaveLoadKey = 0,
        .isMuted = 0,
        .menuPos = 0,
        .savedDemon = {0},
        .ddrHighScores = {{{0}}, {{0}}, {{0}}, {{0}}},
        .gitHash = {0},
        .selfTestPassed = false,
    };
    ```
1. For extra safety, also set an initial value for your data in `LoadSettings()` if `SAVE_LOAD_KEY` doesn't match. By default, all initial data will be zeroed out.
    ```
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
            // Save the values
            SaveSettings();
        }
    }
    ```
1. Write a getter and a setter. The getter should simply return the data from the RAM copy of the NVM. The getter should write to the RAM copy of the NVM, then call `SaveSettings()` to actually write it NVM.
    
    You can also perform more complex logic in the getter. For instance, if you are tracking high scores, you may pass a new score to the setter, which may or may not save it to NVM, depending on if the score is higher than the previous ones.
    ```
    bool ICACHE_FLASH_ATTR getSelfTestPass(void)
    {
        return settings.selfTestPassed;
    }

    void ICACHE_FLASH_ATTR setSelfTestPass(bool pass)
    {
        settings.selfTestPassed = pass;
        SaveSettings();
    }
    ```
1. Don't forget to put the declarations in `nvm_interface.h`
    ```
    void ICACHE_FLASH_ATTR setSelfTestPass(bool pass);
    bool ICACHE_FLASH_ATTR getSelfTestPass(void);
    ```
1. Finally `#include "nvm_interface.h"` wherever you'd like, and save & load persistent data with your new functions.
