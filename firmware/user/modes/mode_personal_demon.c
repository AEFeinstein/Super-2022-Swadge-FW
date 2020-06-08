/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>
#include "mode_personal_demon.h"
#include "assets.h"
#include "oled.h"
#include "linked_list.h"

/*==============================================================================
 * Defines, Enums
 *============================================================================*/

typedef enum
{
    PDA_WALKING,
    PDA_CENTER,
    PDA_EATING,
    PDA_POOPING,
    PDA_MEDICINE,
    PDA_SCOLD,
} pdAnimationState_t;

/*==============================================================================
 * Function Prototypes
 *============================================================================*/

void personalDemonEnterMode(void);
void personalDemonExitMode(void);
void personalDemonButtonCallback(uint8_t state, int button, int down);
void personalDemonAnimationTimer(void* arg __attribute__((unused)));
void personalDemonUpdateDisplay(void);
void personalDemonResetAnimVars(void);

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
    int16_t demonX;
    int16_t demonY;
    bool demonDirLR;
    bool demonDirUD;
    pdAnimationState_t anim;
    syncedTimer_t animationTimer;
    pngSequenceHandle pizza;
    pngSequenceHandle burger;
    pngSequenceHandle* food;
    pngSequenceHandle syringe;
    pngHandle demon;
    pngHandle hand;
    pngHandle poop;
    list_t animationQueue;
    int16_t seqFrame;
    int16_t handRot;
    int16_t animCnt;
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

    pd->demonX = (OLED_WIDTH / 2) - 8;
    pd->demonDirLR = false;
    pd->demonY = (OLED_HEIGHT / 2) - 8;
    pd->demonDirUD = false;

    // Set up an animation timer
    syncedTimerSetFn(&pd->animationTimer, personalDemonAnimationTimer, NULL);
    syncedTimerArm(&pd->animationTimer, 5, true);

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
    if(down)
    {
        switch(button)
        {
            case 0:
            {
                unshift(&pd->animationQueue, (void*)PDA_CENTER);
                unshift(&pd->animationQueue, (void*)PDA_EATING);
                break;
            }
            case 1:
            {
                unshift(&pd->animationQueue, (void*)PDA_CENTER);
                unshift(&pd->animationQueue, (void*)PDA_POOPING);
                break;
            }
            case 2:
            {
                unshift(&pd->animationQueue, (void*)PDA_CENTER);
                unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
                break;
            }
            case 3:
            {
                unshift(&pd->animationQueue, (void*)PDA_CENTER);
                unshift(&pd->animationQueue, (void*)PDA_SCOLD);
                break;
            }
        }
    }
}

/**
 * Timer function for animation
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR personalDemonAnimationTimer(void* arg __attribute__((unused)))
{
    // If the demon is walking, and there's something new to do
    if(pd->anim == PDA_WALKING && pd->animationQueue.length > 0)
    {
        // Start doing it
        pd->anim = (pdAnimationState_t)pop(&(pd->animationQueue));

        // Initialize the animation
        switch(pd->anim)
        {
            case PDA_WALKING:
            {
                break;
            }
            case PDA_CENTER:
            {
                break;
            }
            case PDA_EATING:
            {
                if(os_random() % 2 == 0)
                {
                    pd->food = &pd->burger;
                }
                else
                {
                    pd->food = &pd->pizza;
                }
                pd->demonDirLR = false;
                break;
            }
            case PDA_POOPING:
            {
                break;
            }
            case PDA_MEDICINE:
            {
                break;
            }
            case PDA_SCOLD:
            {
                break;
            }
        }

        // Draw initial frame for this animation
        personalDemonUpdateDisplay();
    }

    switch(pd->anim)
    {
        default:
        case PDA_WALKING:
        {
            // Take one step every 64 frames (320ms)
            if((pd->animCnt++) >= 64)
            {
                pd->animCnt = 0;
                // Check if the demon turns around
                if(os_random() % 64 == 0 || (pd->demonX == OLED_WIDTH - 16) || (pd->demonX == 0))
                {
                    pd->demonDirLR = !(pd->demonDirLR);
                }

                // Move the demon LR
                if(pd->demonDirLR)
                {
                    pd->demonX++;
                }
                else
                {
                    pd->demonX--;
                }

                // Maybe move up or down
                if(os_random() % 4 == 0)
                {
                    // Check if the demon changes up/down direction
                    if(os_random() % 16 == 0 || (pd->demonY == OLED_HEIGHT / 2) || (pd->demonY == (OLED_HEIGHT / 2) - 16))
                    {
                        pd->demonDirUD = !(pd->demonDirUD);
                    }

                    // Move the demon UD
                    if(pd->demonDirUD)
                    {
                        pd->demonY++;
                    }
                    else
                    {
                        pd->demonY--;
                    }
                }
                personalDemonUpdateDisplay();
            }
            break;
        }
        case PDA_CENTER:
        {
            if((pd->animCnt++) >= 8)
            {
                pd->animCnt = 0;
                bool centeredX = false;
                bool centeredY = false;

                if(pd->demonX > (OLED_WIDTH / 2) - 8)
                {
                    pd->demonDirLR = false;
                    pd->demonX -= 2;
                    if(pd->demonX < (OLED_WIDTH / 2) - 8)
                    {
                        pd->demonX = (OLED_WIDTH / 2) - 8;
                    }
                }
                else if(pd->demonX < (OLED_WIDTH / 2) - 8)
                {
                    pd->demonDirLR = true;
                    pd->demonX += 2;
                    if(pd->demonX > (OLED_WIDTH / 2) - 8)
                    {
                        pd->demonX = (OLED_WIDTH / 2) - 8;
                    }
                }
                else
                {
                    centeredX = true;
                }

                if(pd->demonY > (OLED_HEIGHT / 2) - 8)
                {
                    pd->demonY--;
                }
                else if(pd->demonY < (OLED_HEIGHT / 2) - 8)
                {
                    pd->demonY++;
                }
                else
                {
                    centeredY = true;
                }

                if(centeredX && centeredY)
                {
                    personalDemonResetAnimVars();
                }
                personalDemonUpdateDisplay();
            }
            break;
        }
        case PDA_EATING:
        {
            if(pd->animCnt == 100)
            {
                pd->demonX = (OLED_WIDTH / 2) - 8;
                personalDemonUpdateDisplay();
                if(pd->seqFrame == pd->burger.count)
                {
                    personalDemonResetAnimVars();
                }
            }

            if((pd->animCnt++) >= 200)
            {
                pd->animCnt = 0;

                pd->demonX = (OLED_WIDTH / 2) - 12;
                pd->seqFrame++;
                personalDemonUpdateDisplay();
            }
            break;
        }
        case PDA_POOPING:
        {
            break;
        }
        case PDA_MEDICINE:
        {
            if((pd->animCnt++) >= 20)
            {
                pd->animCnt = 0;

                pd->seqFrame++;
                if(pd->seqFrame == pd->syringe.count)
                {
                    personalDemonResetAnimVars();
                }
                personalDemonUpdateDisplay();
            }
            break;
        }
        case PDA_SCOLD:
        {
            if((pd->animCnt++) >= 1)
            {
                pd->animCnt = 0;

                pd->handRot = (pd->handRot + 1) % 360;
                if(pd->handRot == 0)
                {
                    personalDemonResetAnimVars();
                }
                personalDemonUpdateDisplay();
            }
            break;
        }
    }
}

/**
 * @brief TODO
 */
void ICACHE_FLASH_ATTR personalDemonResetAnimVars(void)
{
    pd->animCnt = 0;
    pd->seqFrame = 0;
    pd->anim = PDA_WALKING;
}

/**
 * Update the OLED
 */
void ICACHE_FLASH_ATTR personalDemonUpdateDisplay(void)
{
    // Clear everything
    clearDisplay();

    // Draw the demon
    drawPng((&pd->demon), pd->demonX, pd->demonY, pd->demonDirLR, false, 0);

    // Draw animation-specific elements
    switch(pd->anim)
    {
        case PDA_WALKING:
        case PDA_CENTER:
        {
            // Nothing extra to draw
            break;
        }
        case PDA_EATING:
        {
            drawPngSequence(pd->food,  (OLED_WIDTH / 2) - 28,  (OLED_HEIGHT / 2) - 8, false, false, 0, pd->seqFrame);
            break;
        }
        case PDA_POOPING:
        {
            drawPng((&pd->poop), 20, 0, false, false, 0);
            break;
        }
        case PDA_MEDICINE:
        {
            drawPngSequence(&(pd->syringe), 40, 48, false, false, 0, pd->seqFrame);
            break;
        }
        case PDA_SCOLD:
        {
            drawPng((&pd->hand), 40, 10, false, false, pd->handRot);
            break;
        }
    }
}
