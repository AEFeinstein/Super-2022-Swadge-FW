/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>
#include "mode_personal_demon.h"
#include "assets.h"
#include "oled.h"

/*==============================================================================
 * Defines
 *============================================================================*/

/*==============================================================================
 * Function Prototypes
 *============================================================================*/

void personalDemonEnterMode(void);
void personalDemonExitMode(void);
void personalDemonButtonCallback(uint8_t state, int button, int down);
void personalDemonAnimationTimer(void* arg __attribute__((unused)));
void personalDemonUpdateDisplay(void);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode personalDemonMode =
{
    .modeName = "Personal Demon",
    .fnEnterMode = personalDemonEnterMode,
    .fnExitMode = personalDemonExitMode,
    .fnButtonCallback = personalDemonButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL
};

typedef struct
{
    syncedTimer_t animationTimer;
    pngSequenceHandle pizza;
    pngSequenceHandle burger;
    pngSequenceHandle syringe;
    pngHandle demon;
    pngHandle hand;
    pngHandle poop;
} pd_data;

pd_data* pd;

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initialize the personalDemon mode
 */
void ICACHE_FLASH_ATTR personalDemonEnterMode(void)
{
    // Allocate and zero RAM for this mode
    pd = (pd_data*)os_malloc(sizeof(pd_data));
    ets_memset(pd, 0, sizeof(pd_data));

    allocPngSequence(&(pd->pizza), 3,
                     "pizza1.png",
                     "pizza2.png",
                     "pizza3.png");
    allocPngSequence(&(pd->burger), 3,
                     "burger1.png",
                     "burger2.png",
                     "burger3.png");
    allocPngSequence(&(pd->syringe), 11,
                     "syringe01.png",
                     "syringe02.png",
                     "syringe03.png",
                     "syringe04.png",
                     "syringe05.png",
                     "syringe06.png",
                     "syringe07.png",
                     "syringe08.png",
                     "syringe09.png",
                     "syringe10.png",
                     "syringe11.png");
    allocPngAsset("dino.png", &(pd->demon));
    allocPngAsset("scold.png", &(pd->hand));
    allocPngAsset("poop.png", &(pd->poop));

    // Set up an animation timer
    syncedTimerSetFn(&pd->animationTimer, personalDemonAnimationTimer, NULL);
    syncedTimerArm(&pd->animationTimer, 200, true);

    // Draw the initial display
    personalDemonUpdateDisplay();
}

/**
 * De-initialize the personalDemon mode
 */
void ICACHE_FLASH_ATTR personalDemonExitMode(void)
{
    freePngAsset(&(pd->demon));
    freePngAsset(&(pd->hand));
    freePngAsset(&(pd->poop));
    freePngSequence(&(pd->burger));
    freePngSequence(&(pd->pizza));
    freePngSequence(&(pd->syringe));
    os_free(pd);
}

/**
 * personalDemon mode button press handler. Either start connections or send messages
 * depending on the current state
 *
 * @param state  A bitmask of all buttons currently
 * @param button The button that changed state
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR personalDemonButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    // TODO
}

/**
 * Timer function for animation
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR personalDemonAnimationTimer(void* arg __attribute__((unused)))
{
    // TODO
    personalDemonUpdateDisplay();
}

/**
 * Update the OLED
 */
void ICACHE_FLASH_ATTR personalDemonUpdateDisplay(void)
{
    // Clear everything
    clearDisplay();

    drawPng((&pd->demon), 0, 0, false, false, 0);
    drawPng((&pd->poop), 20, 0, false, false, 0);
    static int handRot = 0;
    handRot = (handRot + 2) % 360;
    drawPng((&pd->hand), 40, 10, false, false, handRot);

    drawPngSequence(&(pd->burger),  0,  48, false, false, 0);
    drawPngSequence(&(pd->pizza),   20, 48, false, false, 0);
    drawPngSequence(&(pd->syringe), 40, 48, false, false, 0);
}
