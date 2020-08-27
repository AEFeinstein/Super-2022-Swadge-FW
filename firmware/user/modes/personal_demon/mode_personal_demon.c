/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>
#include "mode_personal_demon.h"
#include "assets.h"
#include "oled.h"
#include "linked_list.h"
#include "font.h"
#include "logic_personal_demon.h"

/*==============================================================================
 * Defines, Enums
 *============================================================================*/

typedef enum
{
    PDA_WALKING,
    PDA_CENTER,
    PDA_EATING,
    PDA_OVER_EATING,
    PDA_NOT_EATING,
    PDA_POOPING,
    PDA_MEDICINE,
    PDA_SCOLD,
    PDA_BIRTH,
    PDA_NUM_ANIMATIONS
} pdAnimationState_t;

typedef enum
{
    PDM_FEED,
    PDM_PLAY,
    PDM_SCOLD,
    PDM_MEDS,
    PDM_SCOOP,
    PDM_QUIT,
    PDM_NUM_OPTS
} pdMenuOpt_t;

typedef enum
{
    TEXT_STATIC,
    TEXT_MOVING_RIGHT,
    TEXT_MOVING_LEFT
} pdTextAnimationState_t;

typedef struct
{
    void (*initAnim)(void);
    bool (*updtAnim)(void);
    void (*drawAnim)(void);
} pdAnimation;

typedef struct
{
    char* name;
    action_t menuAct;
} pdMenuOpt;

typedef struct
{
    // The demon
    demon_t demon;

    // Loaded Assets
    pngSequenceHandle pizza;
    pngSequenceHandle burger;
    pngSequenceHandle* food;
    pngSequenceHandle syringe;
    pngHandle demonSprite;
    pngHandle hand;
    pngHandle poop;
    pngHandle archL;
    pngHandle archR;

    // Demon position and direction
    int16_t demonX;
    int16_t demonY;
    bool demonDirLR;
    bool demonDirUD;
    int16_t demonRot;

    // Animation variables
    pdAnimationState_t anim;
    pdTextAnimationState_t textAnimation;
    timer_t animationTimer;
    list_t animationQueue;
    pdAnimation animTable[PDA_NUM_ANIMATIONS];
    int16_t seqFrame;
    int16_t handRot;
    int16_t animCnt;
    int16_t drawPoopCnt;
    uint8_t menuIdx;
    int16_t textPos;
    uint8_t numFood;

    pdMenuOpt menuTable[PDM_NUM_OPTS];
} pd_data;

/*==============================================================================
 * Function Prototypes
 *============================================================================*/

void personalDemonEnterMode(void);
void personalDemonExitMode(void);
void personalDemonButtonCallback(uint8_t state, int button, int down);
void personalDemonAnimationTimer(void* arg __attribute__((unused)));
void personalDemonUpdateDisplay(void);
void personalDemonResetAnimVars(void);

bool updtAnimWalk(void);
bool updtAnimCenter(void);
void drawAnimDemon(void);

bool updtAnimText(void);
void drawAnimText(void);

void initAnimEating(void);
bool updtAnimEating(void);
void drawAnimEating(void);

void initAnimOverEating(void);

void initAnimNotEating(void);
bool updtAnimNotEating(void);
void drawAnimNotEating(void);

void initAnimPoop(void);
bool updtAnimPoop(void);

void initAnimMeds(void);
bool updtAnimMeds(void);
void drawAnimMeds(void);

void initAnimScold(void);
bool updtAnimScold(void);
void drawAnimScold(void);

void initAnimPortal(void);
bool updtAnimPortal(void);
void drawAnimPortal(void);

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
    .fnAudioCallback = NULL,
    .menuImg = "demon-menu.gif"
};

pd_data* pd;

char menuFeed[]  = "Feed";
char menuPlay[]  = "Play";
char menuScold[] = "Scold";
char menuMeds[]  = "Meds";
char menuScoop[] = "Scoop";
char menuQuit[]  = "Quit";

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

    resetDemon(&pd->demon);

    // Set up the animation table
    pd->animTable[PDA_WALKING].initAnim = NULL;
    pd->animTable[PDA_WALKING].updtAnim = updtAnimWalk;
    pd->animTable[PDA_WALKING].drawAnim = drawAnimDemon;

    pd->animTable[PDA_CENTER].initAnim = NULL;
    pd->animTable[PDA_CENTER].updtAnim = updtAnimCenter;
    pd->animTable[PDA_CENTER].drawAnim = drawAnimDemon;

    pd->animTable[PDA_EATING].initAnim = initAnimEating;
    pd->animTable[PDA_EATING].updtAnim = updtAnimEating;
    pd->animTable[PDA_EATING].drawAnim = drawAnimEating;

    // Only differs from normal eating in initialization
    pd->animTable[PDA_OVER_EATING].initAnim = initAnimOverEating;
    pd->animTable[PDA_OVER_EATING].updtAnim = updtAnimEating;
    pd->animTable[PDA_OVER_EATING].drawAnim = drawAnimEating;

    pd->animTable[PDA_NOT_EATING].initAnim = initAnimNotEating;
    pd->animTable[PDA_NOT_EATING].updtAnim = updtAnimNotEating;
    pd->animTable[PDA_NOT_EATING].drawAnim = drawAnimNotEating;

    pd->animTable[PDA_POOPING].initAnim = initAnimPoop;
    pd->animTable[PDA_POOPING].updtAnim = updtAnimPoop;
    pd->animTable[PDA_POOPING].drawAnim = drawAnimDemon;

    pd->animTable[PDA_MEDICINE].initAnim = initAnimMeds;
    pd->animTable[PDA_MEDICINE].updtAnim = updtAnimMeds;
    pd->animTable[PDA_MEDICINE].drawAnim = drawAnimMeds;

    pd->animTable[PDA_SCOLD].initAnim = initAnimScold;
    pd->animTable[PDA_SCOLD].updtAnim = updtAnimScold;
    pd->animTable[PDA_SCOLD].drawAnim = drawAnimScold;

    pd->animTable[PDA_BIRTH].initAnim = initAnimPortal;
    pd->animTable[PDA_BIRTH].updtAnim = updtAnimPortal;
    pd->animTable[PDA_BIRTH].drawAnim = drawAnimPortal;

    // Set up the menu table
    pd->menuTable[PDM_FEED].name = menuFeed;
    pd->menuTable[PDM_FEED].menuAct = ACT_FEED;

    pd->menuTable[PDM_PLAY].name = menuPlay;
    pd->menuTable[PDM_PLAY].menuAct = ACT_PLAY;

    pd->menuTable[PDM_SCOLD].name = menuScold;
    pd->menuTable[PDM_SCOLD].menuAct = ACT_DISCIPLINE;

    pd->menuTable[PDM_MEDS].name = menuMeds;
    pd->menuTable[PDM_MEDS].menuAct = ACT_MEDICINE;

    pd->menuTable[PDM_SCOOP].name = menuScoop;
    pd->menuTable[PDM_SCOOP].menuAct = ACT_SCOOP;

    pd->menuTable[PDM_QUIT].name = menuQuit;
    pd->menuTable[PDM_QUIT].menuAct = ACT_QUIT;

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
    allocPngAsset("pd-norm.png", &(pd->demonSprite));
    allocPngAsset("scold.png", &(pd->hand));
    allocPngAsset("poop.png", &(pd->poop));
    allocPngAsset("archL.png", &(pd->archL));
    allocPngAsset("archR.png", &(pd->archR));

    pd->demonX = (OLED_WIDTH / 2) - 8;
    pd->demonDirLR = false;
    pd->demonY = (OLED_HEIGHT / 2) - 8;
    pd->demonDirUD = false;

    // Set up an animation timer
    timerSetFn(&pd->animationTimer, personalDemonAnimationTimer, NULL);
    timerArm(&pd->animationTimer, 5, true);

    // Draw the initial display
    personalDemonAnimationTimer(NULL);
    personalDemonUpdateDisplay();
}

/**
 * De-initialize the personalDemon mode
 */
void ICACHE_FLASH_ATTR personalDemonExitMode(void)
{
    freePngSequence(&(pd->pizza));
    freePngSequence(&(pd->burger));
    freePngSequence(&(pd->syringe));
    freePngAsset(&(pd->demonSprite));
    freePngAsset(&(pd->hand));
    freePngAsset(&(pd->poop));
    freePngAsset(&(pd->archL));
    freePngAsset(&(pd->archR));
    timerDisarm(&pd->animationTimer);
    timerFlush();
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
    if(down)
    {
        switch(button)
        {
            case 0:
            {
                pd->textAnimation = TEXT_MOVING_RIGHT;
                break;
            }
            case 1:
            {
                break;
            }
            case 2:
            {
                pd->textAnimation = TEXT_MOVING_LEFT;
                break;
            }
            case 3:
            {
                break;
            }
            case 4:
            {
                takeAction(&pd->demon, pd->menuTable[pd->menuIdx].menuAct);
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
        if(NULL != pd->animTable[pd->anim].initAnim)
        {
            pd->animTable[pd->anim].initAnim();
        }
    }

    // Draw anything else for this scene
    if(pd->animTable[pd->anim].updtAnim() || updtAnimText())
    {
        personalDemonUpdateDisplay();
    }
}

/**
 * Update the OLED
 */
void ICACHE_FLASH_ATTR personalDemonUpdateDisplay(void)
{
    // Clear everything
    clearDisplay();

    // Always draw poop if it's there
    for(uint8_t py = 0; py < 2; py++)
    {
        for(uint8_t px = 0; px < 3; px++)
        {
            if(px + (py * 3) < pd->drawPoopCnt)
            {
                drawPng((&pd->poop), (18 * px) + (OLED_WIDTH / 2) + 12, (14 * py) + (OLED_HEIGHT / 2) - 6, false, false, 0);
            }
        }
    }

    // Draw anything else for this scene
    if(NULL != pd->animTable[pd->anim].drawAnim)
    {
        pd->animTable[pd->anim].drawAnim();
    }

    // Draw text
    drawAnimText();
}

/**
 * @brief TODO
 */
void ICACHE_FLASH_ATTR personalDemonResetAnimVars(void)
{
    pd->animCnt = 0;
    pd->seqFrame = 0;
    pd->handRot = 0;
    pd->demonRot = 0;
    pd->anim = PDA_WALKING;
}

/*******************************************************************************
 * General Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 * @param evt
 */
void ICACHE_FLASH_ATTR animateEvent(event_t evt)
{
    switch(evt)
    {
        case EVT_GOT_SICK_RANDOMLY:
        {
            os_printf("%s randomly got sick\n", pd->demon.name);
            break;
        }
        case EVT_GOT_SICK_POOP:
        {
            os_printf("Poop made %s sick\n", pd->demon.name);
            break;
        }
        case EVT_GOT_SICK_OBESE:
        {
            os_printf("Obesity made %s sick\n", pd->demon.name);
            break;
        }
        case EVT_GOT_SICK_MALNOURISHED:
        {
            os_printf("Malnourishment made %s sick\n", pd->demon.name);
            break;
        }
        case EVT_POOPED:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_POOPING);
            os_printf("%s pooped\n", pd->demon.name);
            break;
        }
        case EVT_LOST_DISCIPLINE:
        {
            os_printf("%s became less disciplined\n", pd->demon.name);
            break;
        }
        case EVT_EAT:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_EATING);
            os_printf("%s ate the food\n", pd->demon.name);
            break;
        }
        case EVT_OVEREAT:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_OVER_EATING);
            os_printf("%s ate the food, then stole more and overate\n", pd->demon.name);
            break;
        }
        case EVT_NO_EAT_SICK:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_EATING);
            os_printf("%s was too sick to eat\n", pd->demon.name);
            break;
        }
        case EVT_NO_EAT_DISCIPLINE:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_EATING);
            os_printf("%s was too unruly eat\n", pd->demon.name);
            break;
        }
        case EVT_NO_EAT_FULL:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_EATING);
            os_printf("%s was too full to eat\n", pd->demon.name);
            break;
        }
        case EVT_PLAY:
        {
            os_printf("You played with %s\n", pd->demon.name);
            break;
        }
        case EVT_NO_PLAY_DISCIPLINE:
        {
            os_printf("%s was too unruly to play\n", pd->demon.name);
            break;
        }
        case EVT_SCOLD:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_SCOLD);
            os_printf("You scolded %s\n", pd->demon.name);
            break;
        }
        case EVT_NO_SCOLD_SICK:
        {
            os_printf("You scolded %s, but it was sick\n", pd->demon.name);
            break;
        }
        case EVT_MEDICINE_NOT_SICK:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
            os_printf("You gave %s medicine, but it wasn't sick\n", pd->demon.name);
            break;
        }
        case EVT_MEDICINE_CURE:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
            os_printf("You gave %s medicine, and it was cured\n", pd->demon.name);
            break;
        }
        case EVT_MEDICINE_FAIL:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
            os_printf("You gave %s medicine, but it didn't work\n", pd->demon.name);
            break;
        }
        case EVT_FLUSH_POOP:
        {
            os_printf("You flushed a poop\n");
            break;
        }
        case EVT_FLUSH_NOTHING:
        {
            os_printf("You flushed nothing\n");
            break;
        }
        case EVT_LOST_HEALTH_SICK:
        {
            os_printf("%s lost health to sickness\n", pd->demon.name);
            break;
        }
        case EVT_LOST_HEALTH_OBESITY:
        {
            os_printf("%s lost health to obesity\n", pd->demon.name);
            break;
        }
        case EVT_LOST_HEALTH_MALNOURISHMENT:
        {
            os_printf("%s lost health to malnourishment\n", pd->demon.name);
            break;
        }
        case EVT_TEENAGER:
        {
            os_printf("%s is now a teenager. Watch out.\n", pd->demon.name);
            break;
        }
        case EVT_ADULT:
        {
            os_printf("%s is now an adult. Boring.\n", pd->demon.name);
            break;
        }
        case EVT_BORN:
        {
            unshift(&pd->animationQueue, (void*)PDA_BIRTH);
            os_printf("%s fell out of a portal\n", pd->demon.name);
            break;
        }
        case EVT_DEAD:
        {
            os_printf("%s died\n", pd->demon.name);
            break;
        }
        default:
        case EVT_NONE:
        case EVT_NUM_EVENTS:
        {
            break;
        }
    }
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimWalk(void)
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
        return true;
    }
    return false;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimCenter(void)
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
        return true;
    }
    return false;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimDemon(void)
{
    // Draw the demon
    drawPng((&pd->demonSprite), pd->demonX, pd->demonY, pd->demonDirLR, false, pd->demonRot);
}

/*******************************************************************************
 * Eating Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimEating(void)
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
    pd->numFood = 1;
}

/**
 * @brief
 *
 */
bool ICACHE_FLASH_ATTR updtAnimEating(void)
{
    bool shouldDraw = false;
    if(pd->animCnt == 100)
    {
        pd->demonX = (OLED_WIDTH / 2) - 8;
        shouldDraw = true;
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
        shouldDraw = true;
    }
    return shouldDraw;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimEating(void)
{
    // Draw the demon
    drawAnimDemon();
    // Draw the food
    if(pd->numFood == 1)
    {
        drawPngSequence(pd->food,  (OLED_WIDTH / 2) - 28,  (OLED_HEIGHT / 2) - 8, false, false, 0, pd->seqFrame);
    }
    else
    {
        drawPngSequence(pd->food,  (OLED_WIDTH / 2) - 28,  (OLED_HEIGHT / 2) - 16, false, false, 0, pd->seqFrame);
        drawPngSequence(pd->food,  (OLED_WIDTH / 2) - 46,  (OLED_HEIGHT / 2) - 16, false, false, 0, pd->seqFrame);
        drawPngSequence(pd->food,  (OLED_WIDTH / 2) - 37,  (OLED_HEIGHT / 2) + 2,  false, false, 0, pd->seqFrame);
    }

}

/*******************************************************************************
 * Overeating Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimOverEating(void)
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
    pd->numFood = 3;
}

/*******************************************************************************
 * Not Eating Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimNotEating(void)
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
}

/**
 * @brief
 *
 */
bool ICACHE_FLASH_ATTR updtAnimNotEating(void)
{
    bool shouldDraw = false;

    if(0 == pd->animCnt)
    {
        shouldDraw = true;
    }
    else if(pd->animCnt % 100 == 0)
    {
        pd->demonDirLR = !pd->demonDirLR;
        shouldDraw = true;
    }

    if((pd->animCnt++) == 600)
    {
        personalDemonResetAnimVars();
        shouldDraw = true;
    }
    return shouldDraw;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimNotEating(void)
{
    // Draw the demon
    drawAnimDemon();
    // Draw the food
    drawPngSequence(pd->food,  (OLED_WIDTH / 2) - 28,  (OLED_HEIGHT / 2) - 8, false, false, 0, 0);
}

/*******************************************************************************
 * Poop Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimPoop(void)
{
    pd->demonDirLR = false;
    pd->handRot = 0;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimPoop(void)
{
    pd->animCnt++;

    if(pd->animCnt >= 180)
    {
        personalDemonResetAnimVars();
    }
    if(pd->animCnt >= 90)
    {
        if(pd->animCnt == 90)
        {
            pd->drawPoopCnt++;
        }
        if(pd->demonRot == 359)
        {
            pd->demonRot = 0;
        }
        else
        {
            pd->demonRot++;
        }
    }
    else
    {
        if(pd->demonRot == 0)
        {
            pd->demonRot = 359;
        }
        else
        {
            pd->demonRot--;
        }
    }
    return true;
}

/*******************************************************************************
 * Medicine Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimMeds(void)
{
    pd->demonDirLR = false;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimMeds(void)
{
    if((pd->animCnt++) >= 20)
    {
        pd->animCnt = 0;

        if(pd->seqFrame % 2 == 0)
        {
            pd->demonX--;
        }
        else
        {
            pd->demonX++;
        }

        pd->seqFrame++;
        if(pd->seqFrame == pd->syringe.count)
        {
            personalDemonResetAnimVars();
        }
        return true;

    }
    return false;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimMeds(void)
{
    // Draw the demon
    drawAnimDemon();
    // Draw the syringe
    drawPngSequence(&(pd->syringe), (OLED_WIDTH / 2) - 24, (OLED_HEIGHT / 2) - 4, false, false, 0, pd->seqFrame);
}

/*******************************************************************************
 * Scolding Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimScold(void)
{
    pd->demonDirLR = false;
    pd->handRot = 90;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimScold(void)
{
    pd->animCnt++;

    if(pd->animCnt >= 360)
    {
        personalDemonResetAnimVars();
    }
    else if(pd->animCnt >= 270)
    {
        pd->handRot--;
    }
    else if(pd->animCnt >= 180)
    {
        if(pd->animCnt == 250)
        {
            pd->demonDirLR = !pd->demonDirLR;
        }
        pd->handRot++;
    }
    else if(pd->animCnt >= 90)
    {
        if(pd->animCnt == 90)
        {
            pd->demonY += 2;
        }
        pd->handRot--;
    }
    else
    {
        pd->handRot++;
    }
    return true;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimScold(void)
{
    // Draw the demon
    drawAnimDemon();
    // Draw the hand
    drawPng((&pd->hand), (OLED_WIDTH / 2) - 28, (OLED_HEIGHT / 2) - 28, false, false, pd->handRot);
}

/*******************************************************************************
 * Portal Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimPortal(void)
{
    pd->demonDirLR = true;
    pd->demonY = 13 + 38 - 16;
    pd->demonX = 16 + 24 - 16;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimPortal(void)
{
    if((pd->animCnt++) >= 16)
    {
        pd->animCnt = 0;
        pd->demonX++;
        if(pd->demonX % 2 == 1)
        {
            pd->demonY--;
        }
        else
        {
            pd->demonY++;
        }

        if(pd->demonX >= 70)
        {
            personalDemonResetAnimVars();
        }
        return true;
    }
    return false;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimPortal(void)
{
    // Draw half of the portal
    drawPng(&pd->archR, 16 + 24, 13, false, false, 0);
    // Draw the demon
    drawAnimDemon();
    // Draw the other half
    drawPng(&pd->archL, 16, 13, false, false, 0);
}

/*******************************************************************************
 * Text Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimText(void)
{
    switch (pd->textAnimation)
    {
        case TEXT_MOVING_LEFT:
        {
            pd->textPos--;
            if(pd->textPos == -1 * OLED_WIDTH)
            {
                pd->textPos = 0;
                pd->textAnimation = TEXT_STATIC;

                if(pd->menuIdx == PDM_NUM_OPTS - 1)
                {
                    pd->menuIdx = 0;
                }
                else
                {
                    pd->menuIdx++;
                }
            }
            return true;
        }
        case TEXT_MOVING_RIGHT:
        {
            pd->textPos++;
            if(pd->textPos == OLED_WIDTH)
            {
                pd->textPos = 0;
                pd->textAnimation = TEXT_STATIC;

                if(pd->menuIdx == 0)
                {
                    pd->menuIdx = PDM_NUM_OPTS - 1;
                }
                else
                {
                    pd->menuIdx--;
                }
            }
            return true;
        }
        case TEXT_STATIC:
        default:
        {
            return false;
        }
    }
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimText(void)
{
    switch (pd->textAnimation)
    {
        case TEXT_MOVING_LEFT:
        {
            uint8_t next;
            if(pd->menuIdx < PDM_NUM_OPTS - 1)
            {
                next = pd->menuIdx + 1;
            }
            else
            {
                next = 0;
            }
            plotText(pd->textPos, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, pd->menuTable[pd->menuIdx].name, IBM_VGA_8, WHITE);
            plotText(pd->textPos + OLED_WIDTH, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, pd->menuTable[next].name, IBM_VGA_8, WHITE);
            break;
        }
        case TEXT_MOVING_RIGHT:
        {
            uint8_t prev;
            if(pd->menuIdx > 0)
            {
                prev = pd->menuIdx - 1;
            }
            else
            {
                prev = PDM_NUM_OPTS - 1;
            }
            plotText(pd->textPos - OLED_WIDTH, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, pd->menuTable[prev].name, IBM_VGA_8, WHITE);
            plotText(pd->textPos, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, pd->menuTable[pd->menuIdx].name, IBM_VGA_8, WHITE);
            break;
        }
        default:
        case TEXT_STATIC:
        {
            plotText(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8, pd->menuTable[pd->menuIdx].name, IBM_VGA_8, WHITE);
            break;
        }
    }
}
