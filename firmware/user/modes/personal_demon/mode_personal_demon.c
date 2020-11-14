/*==============================================================================
 * Includes
 *============================================================================*/

#include <osapi.h>
#include <mem.h>
#include <user_interface.h>
#include "mode_personal_demon.h"
#include "assets.h"
#include "oled.h"
#include "cndraw.h"
#include "linked_list.h"
#include "font.h"
#include "logic_personal_demon.h"
#include "nvm_interface.h"
#include "menu2d.h"

/*==============================================================================
 * Defines, Enums
 *============================================================================*/

#define ACT_STRLEN 128
#define MAX_BOUNCES 2
#define CLAMP(x,min,max) ( (x) < (min) ? (min) : ((x) > (max) ? (max) : (x)) )

typedef struct
{
    char str[ACT_STRLEN];
    int16_t pos;
} marqueeText_t;

typedef enum
{
    PDA_WALKING,
    PDA_CENTER,
    PDA_EATING,
    PDA_OVER_EATING,
    PDA_NOT_EATING,
    PDA_POOPING,
    PDA_FLUSH,
    PDA_PLAYING,
    PDA_NOT_PLAYING,
    PDA_MEDICINE,
    PDA_SCOLD,
    PDA_BIRTH,
    PDA_DEATH,
    PDA_BIRTHDAY,
    PDA_NUM_ANIMATIONS
} pdAnimationState_t;

typedef struct
{
    void (*initAnim)(void);
    bool (*updtAnim)(uint32_t);
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
    pngHandle demonSpriteFat;
    pngHandle demonSpriteThin;
    pngHandle demonSpriteSick;
    pngHandle hand;
    pngHandle poop;
    pngHandle archL;
    pngHandle archR;
    pngHandle cake;
    pngHandle ball;
    pngHandle water;
    pngHandle heart;

    // Demon position, direction, and state
    int16_t demonX;
    int16_t demonY;
    bool demonDirLR;
    bool demonDirUD;
    int16_t demonRot;
    bool drawThin;
    bool drawFat;
    bool drawSick;
    int16_t drawHealth;
    int32_t ledHappy;
    int32_t ledDiscipline;

    // Animation variables
    timer_t ledTimer;
    pdAnimationState_t anim;
    list_t animationQueue;
    pdAnimation animTable[PDA_NUM_ANIMATIONS];
    int16_t seqFrame;
    float handRot;
    uint32_t animTimeUs;
    int16_t drawPoopCnt;
    uint8_t numFood;
    int16_t flushY;

    float ballX;
    float ballY;
    float ballVelX;
    float ballVelY;

    list_t marqueeTextQueue;

    menu_t* menu;

    bool isDisplayingRecords;
} pd_data;

typedef struct
{
    char* norm;
    char* thin;
    char* fat;
    char* sick;
} demonSprites_t;

/*==============================================================================
 * Function Prototypes
 *============================================================================*/

void personalDemonEnterMode(void);
void personalDemonExitMode(void);
void personalDemonButtonCallback(uint8_t state, int button, int down);
bool personalDemonAnimationRender(void);
void personalDemonUpdateDisplay(void);
void personalDemonResetAnimVars(void);

void personalDemonLedTimer(void*);
static void demonMenuCb(const char* menuItem);

bool updtAnimWalk(uint32_t);
bool updtAnimCenter(uint32_t);
void drawAnimDemon(void);

void initAnimEating(void);
bool updtAnimEating(uint32_t);
void drawAnimEating(void);

void initAnimOverEating(void);

void initAnimNotEating(void);
bool updtAnimNotEating(uint32_t);
void drawAnimNotEating(void);

void initAnimPlaying(void);
bool updtAnimPlaying(uint32_t);
bool updtAnimNotPlaying(uint32_t);
bool _updtAnimPlaying(uint32_t, bool);
void drawAnimPlaying(void);

void initAnimPoop(void);
bool updtAnimPoop(uint32_t);

void initAnimFlush(void);
bool updtAnimFlush(uint32_t);
void drawAnimFlush(void);

void initAnimMeds(void);
bool updtAnimMeds(uint32_t);
void drawAnimMeds(void);

void initAnimScold(void);
bool updtAnimScold(uint32_t);
void drawAnimScold(void);

void initAnimPortal(void);
bool updtAnimPortal(uint32_t);
void drawAnimPortal(void);

void initAnimDeath(void);
bool updtAnimDeath(uint32_t);
void drawAnimDeath(void);

bool updtAnimBirthday(uint32_t);
void drawAnimBirthday(void);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode personalDemonMode =
{
    .modeName = "Personal Demon",
    .fnEnterMode = personalDemonEnterMode,
    .fnExitMode = personalDemonExitMode,
    .fnButtonCallback = personalDemonButtonCallback,
    .fnRenderTask = personalDemonAnimationRender,
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
char menuFlush[] = "Flush";
char menuRecords[] = "Records";
char menuQuit[]  = "Quit";

const demonSprites_t demonSprites[] =
{
    {
        .norm = "pd-1-norm.png",
        .sick = "pd-1-sick.png",
        .thin = "pd-1-thin.png",
        .fat  = "pd-1-fat.png"
    },
    {
        .norm = "pd-2-norm.png",
        .sick = "pd-2-sick.png",
        .thin = "pd-2-thin.png",
        .fat  = "pd-2-fat.png"
    },
    {
        .norm = "pd-3-norm.png",
        .sick = "pd-3-sick.png",
        .thin = "pd-3-thin.png",
        .fat  = "pd-3-fat.png"
    },
    {
        .norm = "pd-4-norm.png",
        .sick = "pd-4-sick.png",
        .thin = "pd-4-thin.png",
        .fat  = "pd-4-fat.png"
    },
};

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

    // Try loading the demon from NVM
    getSavedDemon(&pd->demon);

    if(0 == pd->demon.name[0])
    {
        // Demon not loaded, init from scratch
        resetDemon(&pd->demon);
        // And immediately save it
        setSavedDemon(&(pd->demon));
    }

    // Initialize demon draw state
    pd->drawSick = pd->demon.isSick;
    pd->drawFat = isDemonObese(&(pd->demon));
    pd->drawThin = isDemonThin(&(pd->demon));
    pd->drawHealth = pd->demon.health;
    pd->drawPoopCnt = pd->demon.poopCount;
    pd->ledHappy = CLAMP(pd->demon.happy, -4, 4);
    pd->ledDiscipline = CLAMP(pd->demon.discipline, -4, 4);

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

    pd->animTable[PDA_FLUSH].initAnim = initAnimFlush;
    pd->animTable[PDA_FLUSH].updtAnim = updtAnimFlush;
    pd->animTable[PDA_FLUSH].drawAnim = drawAnimFlush;

    pd->animTable[PDA_PLAYING].initAnim = initAnimPlaying;
    pd->animTable[PDA_PLAYING].updtAnim = updtAnimPlaying;
    pd->animTable[PDA_PLAYING].drawAnim = drawAnimPlaying;

    pd->animTable[PDA_NOT_PLAYING].initAnim = initAnimPlaying;
    pd->animTable[PDA_NOT_PLAYING].updtAnim = updtAnimNotPlaying;
    pd->animTable[PDA_NOT_PLAYING].drawAnim = drawAnimPlaying;

    pd->animTable[PDA_MEDICINE].initAnim = initAnimMeds;
    pd->animTable[PDA_MEDICINE].updtAnim = updtAnimMeds;
    pd->animTable[PDA_MEDICINE].drawAnim = drawAnimMeds;

    pd->animTable[PDA_SCOLD].initAnim = initAnimScold;
    pd->animTable[PDA_SCOLD].updtAnim = updtAnimScold;
    pd->animTable[PDA_SCOLD].drawAnim = drawAnimScold;

    pd->animTable[PDA_BIRTH].initAnim = initAnimPortal;
    pd->animTable[PDA_BIRTH].updtAnim = updtAnimPortal;
    pd->animTable[PDA_BIRTH].drawAnim = drawAnimPortal;

    pd->animTable[PDA_DEATH].initAnim = initAnimDeath;
    pd->animTable[PDA_DEATH].updtAnim = updtAnimDeath;
    pd->animTable[PDA_DEATH].drawAnim = drawAnimDeath;

    pd->animTable[PDA_BIRTHDAY].initAnim = NULL;
    pd->animTable[PDA_BIRTHDAY].updtAnim = updtAnimBirthday;
    pd->animTable[PDA_BIRTHDAY].drawAnim = drawAnimBirthday;


    pd->menu = initMenu(NULL, demonMenuCb);
    addRowToMenu(pd->menu);
    addItemToRow(pd->menu, menuFeed);
    addItemToRow(pd->menu, menuPlay);
    addItemToRow(pd->menu, menuScold);
    addItemToRow(pd->menu, menuMeds);
    addItemToRow(pd->menu, menuFlush);
    addItemToRow(pd->menu, menuRecords);
    addItemToRow(pd->menu, menuQuit);

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
    allocPngAsset(demonSprites[pd->demon.species].norm, &(pd->demonSprite));
    allocPngAsset(demonSprites[pd->demon.species].fat,  &(pd->demonSpriteFat));
    allocPngAsset(demonSprites[pd->demon.species].thin, &(pd->demonSpriteThin));
    allocPngAsset(demonSprites[pd->demon.species].sick, &(pd->demonSpriteSick));
    allocPngAsset("scold.png", &(pd->hand));
    allocPngAsset("poop.png", &(pd->poop));
    allocPngAsset("archL.png", &(pd->archL));
    allocPngAsset("archR.png", &(pd->archR));
    allocPngAsset("cake.png", &(pd->cake));
    allocPngAsset("ball.png", &(pd->ball));
    allocPngAsset("water.png", &(pd->water));
    allocPngAsset("heart.png", &(pd->heart));

    pd->demonX = (OLED_WIDTH / 2) - (pd->demonSprite.width / 2);
    pd->demonDirLR = false;
    pd->demonY = (OLED_HEIGHT / 2) - (pd->demonSprite.height / 2);
    pd->demonDirUD = false;

    pd->isDisplayingRecords = false;

    timerSetFn(&(pd->ledTimer), personalDemonLedTimer, NULL);
    timerArm(&(pd->ledTimer), 10, true);

    // Draw the initial display
    personalDemonAnimationRender();
    // Do it twice to draw after setting the time
    personalDemonAnimationRender();
}

/**
 * @return The number of demon species
 */
uint8_t ICACHE_FLASH_ATTR getNumDemonSpecies(void)
{
    return (sizeof(demonSprites) / sizeof(demonSprites[0]));
}

/**
 * De-initialize the personalDemon mode
 */
void ICACHE_FLASH_ATTR personalDemonExitMode(void)
{
    // Stop the timers
    timerDisarm(&(pd->ledTimer));
    timerFlush();

    // Clear the queues
    ets_memset(&(pd->demon.evQueue), EVT_NONE, sizeof(pd->demon.evQueue));

    while(pd->marqueeTextQueue.length > 0)
    {
        void* node = pop(&(pd->marqueeTextQueue));
        os_free(node);
    }

    while(pd->animationQueue.length > 0)
    {
        pop(&(pd->animationQueue));
    }

    // Free the assets
    freePngSequence(&(pd->pizza));
    freePngSequence(&(pd->burger));
    freePngSequence(&(pd->syringe));
    freePngAsset(&(pd->demonSprite));
    freePngAsset(&(pd->demonSpriteFat));
    freePngAsset(&(pd->demonSpriteSick));
    freePngAsset(&(pd->demonSpriteThin));
    freePngAsset(&(pd->hand));
    freePngAsset(&(pd->poop));
    freePngAsset(&(pd->archL));
    freePngAsset(&(pd->archR));
    freePngAsset(&(pd->cake));
    freePngAsset(&(pd->ball));
    freePngAsset(&(pd->water));
    freePngAsset(&(pd->heart));

    // Free the menu
    deinitMenu(pd->menu);

    // Free the memory
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
        int button, int down __attribute__((unused)))
{
    if(down)
    {
        // If any button is pressed while the records are displayed
        if(pd->isDisplayingRecords)
        {
            // Start animating again
            pd->isDisplayingRecords = false;
        }
        else if(pd->anim == PDA_WALKING)
        {
            menuButton(pd->menu, button);
        }
    }
}

/**
 * @brief
 *
 * @param menuItem
 */
static void ICACHE_FLASH_ATTR demonMenuCb(const char* menuItem)
{
    if(menuItem == menuFeed)
    {
        takeAction(&(pd->demon), ACT_FEED);
    }
    else if(menuItem == menuPlay)
    {
        takeAction(&(pd->demon), ACT_PLAY);
    }
    else if(menuItem == menuScold)
    {
        takeAction(&(pd->demon), ACT_DISCIPLINE);
    }
    else if(menuItem == menuMeds)
    {
        takeAction(&(pd->demon), ACT_MEDICINE);
    }
    else if(menuItem == menuFlush)
    {
        takeAction(&(pd->demon), ACT_FLUSH);
    }
    else if(menuItem == menuRecords)
    {
        pd->isDisplayingRecords = true;
    }
    else if(menuItem == menuQuit)
    {
        // Save before quitting, otherwise pd->demon is free()'d
        setSavedDemon(&(pd->demon));
        takeAction(&(pd->demon), ACT_QUIT);
        return;
    }

    // Save after the action was taken
    setSavedDemon(&(pd->demon));
}

/**
 * @brief LED update function, called on a timer
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR personalDemonLedTimer(void* arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};

    if(pd->drawSick)
    {
        leds[LED_3].r = 0xFF;
        leds[LED_4].r = 0xFF;
    }

    // +/-4
    leds[LED_1].r = CLAMP((255 * (4 - pd->ledDiscipline)) / 9, 0, 255);
    leds[LED_1].g = CLAMP((255 * (4 + pd->ledDiscipline)) / 9, 0, 255);
    leds[LED_1].b = 0;
    leds[LED_6].r = leds[LED_1].r;
    leds[LED_6].g = leds[LED_1].g;
    leds[LED_6].b = leds[LED_1].b;

    // +/-4
    leds[LED_2].r = CLAMP((255 * (4 - pd->ledHappy)) / 9, 0, 255);
    leds[LED_2].g = 0;
    leds[LED_2].b = CLAMP((255 * (4 + pd->ledHappy)) / 9, 0, 255);
    leds[LED_5].r = leds[LED_2].r;
    leds[LED_5].g = leds[LED_2].g;
    leds[LED_5].b = leds[LED_2].b;

    // Dim everything a bit
    for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
    {
        leds[i].r >>= 2;
        leds[i].g >>= 2;
        leds[i].b >>= 2;
    }

    setLeds(leds, sizeof(leds));
}

/**
 * Timer function for animation
 *
 * @param arg unused
 */
bool ICACHE_FLASH_ATTR personalDemonAnimationRender(void)
{
    static uint32_t tLastCallUs = 0;

    if(0 == tLastCallUs)
    {
        tLastCallUs = system_get_time();
    }
    else
    {
        uint32_t tNow = system_get_time();
        uint32_t tElapsed = tNow - tLastCallUs;
        tLastCallUs = tNow;

        if(pd->isDisplayingRecords)
        {
            // Show the memorials instead
            clearDisplay();

            // Get the records from NVM
            demonMemorial_t* memorials = getDemonMemorials();

            bool memorialsDrawn = false;

            // There's space to draw five rows
            for(int i = 0; i < 5; i++)
            {
                // If there's an entry
                if(memorials[i].actionsTaken > 0 && memorials[i].name[0] != 0)
                {
                    // Plot the name, left justified
                    plotText(0, 1 + i * (FONT_HEIGHT_IBMVGA8 + 2), memorials[i].name, IBM_VGA_8, WHITE);

                    // Plot the number of actions, right justified
                    char actionsTaken[8] = {0};
                    ets_snprintf(actionsTaken, sizeof(actionsTaken), "%d", memorials[i].actionsTaken);
                    int16_t width = textWidth(actionsTaken, IBM_VGA_8);
                    plotText(OLED_WIDTH - width, 1 + i * (FONT_HEIGHT_IBMVGA8 + 2), actionsTaken, IBM_VGA_8, WHITE);

                    memorialsDrawn = true;
                }
            }

            if(false == memorialsDrawn)
            {
                char text[] = "No Records";
                int16_t width = textWidth(text, IBM_VGA_8);
                plotText((OLED_WIDTH - width) / 2, (OLED_HEIGHT - FONT_HEIGHT_IBMVGA8) / 2, text, IBM_VGA_8, WHITE);
            }

            // Note that the records are being displayed
            pd->isDisplayingRecords = true;
        }
        else
        {
            // If the demon is walking
            if(pd->anim == PDA_WALKING)
            {
                // and there's something new to do
                if(pd->animationQueue.length > 0)
                {
                    // Start doing it
                    pd->anim = (pdAnimationState_t)pop(&(pd->animationQueue));

                    // Initialize the animation
                    if(NULL != pd->animTable[pd->anim].initAnim)
                    {
                        pd->animTable[pd->anim].initAnim();
                    }
                }
                else
                {
                    // If all animations are finished, update the draw state
                    pd->drawSick = pd->demon.isSick;
                    pd->drawFat = isDemonObese(&(pd->demon));
                    pd->drawThin = isDemonThin(&(pd->demon));
                    pd->drawHealth = pd->demon.health;
                    pd->ledHappy = CLAMP(pd->demon.happy, -4, 4);
                    pd->ledDiscipline = CLAMP(pd->demon.discipline, -4, 4);
                }
            }

            // Draw anything else for this scene
            pd->animTable[pd->anim].updtAnim(tElapsed);
            personalDemonUpdateDisplay();

            // Draw the menu text for this scene
            drawMenu(pd->menu);

            // Only draw health if the demon is alive
            if(pd->demon.health)
            {
                int16_t healthPxCovered = 40 - (pd->drawHealth * 40) / STARTING_HEALTH;
                if(healthPxCovered > 40)
                {
                    healthPxCovered = 40;
                }

                // Always draw the health counter
                for(uint8_t i = 0; i < 4; i++)
                {
                    drawPng(&(pd->heart),
                            OLED_WIDTH - pd->heart.width,
                            FONT_HEIGHT_IBMVGA8 + 1 + i * (pd->heart.height),
                            false, false, 0);
                }

                if(healthPxCovered)
                {
                    fillDisplayArea(OLED_WIDTH - pd->heart.width, FONT_HEIGHT_IBMVGA8 + 1,
                                    OLED_WIDTH, FONT_HEIGHT_IBMVGA8 + healthPxCovered,
                                    BLACK);
                }
            }

            // Draw the menu text for this screen
            // Shift the text 1px every 20ms
            static uint32_t marqueeTextAccum = 0;
            marqueeTextAccum += tElapsed;
            int16_t pxToShift = 0;
            while(marqueeTextAccum > 20000)
            {
                pxToShift++;
                marqueeTextAccum -= 20000;
            }

            // If there's anything in the text marquee queue
            if(pd->marqueeTextQueue.length > 0 && pxToShift > 0)
            {
                // Clear the text background first
                fillDisplayArea(0, 0, OLED_WIDTH, FONT_HEIGHT_IBMVGA8, BLACK);
                // Iterate through all the text
                node_t* node = pd->marqueeTextQueue.first;

                // Shift all the text
                while(NULL != node)
                {
                    // Get the text from the queue
                    marqueeText_t* text = node->val;

                    // Iterate to the next
                    node = node->next;

                    // Shift the text if it's time
                    text->pos -= pxToShift;
                }

                // Then draw the necessary text
                node = pd->marqueeTextQueue.first;
                while(NULL != node)
                {
                    // Get the text from the queue
                    marqueeText_t* text = node->val;

                    // Iterate to the next
                    node = node->next;

                    // Plot the text that's on the OLED
                    if(text->pos >= OLED_WIDTH)
                    {
                        // Out of bounds, so return
                        return true;
                    }
                    else if (0 > plotText(text->pos, 0, text->str, IBM_VGA_8, WHITE))
                    {
                        // If the text was plotted off the screen, remove it from the queue
                        shift(&(pd->marqueeTextQueue));
                        os_free(text);
                    }
                }
            }
        }
    }
    return true;
}

/**
 * Update the OLED
 */
void ICACHE_FLASH_ATTR personalDemonUpdateDisplay(void)
{
    // Clear everything
    clearDisplay();

    // Always draw poop if it's there
    for(uint8_t p = 0; p < pd->drawPoopCnt; p++)
    {
        // If flushing, draw the first poop over the water (last)
        if(0 == p && pd->anim == PDA_FLUSH)
        {
            continue;
        }
        int16_t x = (OLED_WIDTH / 2) + (pd->demonSprite.width / 2) + p * ((pd->poop.width / 2) + 1);
        int16_t y;
        if(0 == p % 2)
        {
            y = (OLED_HEIGHT / 2) - (pd->poop.height) - 2;
        }
        else
        {
            y = (OLED_HEIGHT / 2) + 2;
        }

        drawPng((&pd->poop), x, y, false, false, 0);
    }

    // Draw anything else for this scene
    if(NULL != pd->animTable[pd->anim].drawAnim)
    {
        pd->animTable[pd->anim].drawAnim();
    }

    // If flushing, draw the first poop over the water (last)
    if(pd->anim == PDA_FLUSH && pd->drawPoopCnt > 0)
    {
        int16_t x = (OLED_WIDTH / 2) + (pd->demonSprite.width / 2);
        int16_t y = (OLED_HEIGHT / 2) - (pd->poop.height) - 2;
        int16_t watersEnd = pd->flushY + 3 * (pd->water.height);
        // If the water is at the end of the poop, flush it away
        if(watersEnd > y + pd->poop.height)
        {
            y = watersEnd - pd->poop.height;
        }
        drawPng((&pd->poop), x, y, false, false, 0);
    }
}

/**
 * @brief TODO
 */
void ICACHE_FLASH_ATTR personalDemonResetAnimVars(void)
{
    pd->animTimeUs = 0;
    pd->seqFrame = 0;
    pd->handRot = 0;
    pd->demonRot = 0;
    pd->anim = PDA_WALKING;
    pd->ballX = 0;
    pd->ballY = 0;
    pd->ballVelX = 0;
    pd->ballVelY = 0;
    pd->flushY = 0;
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
    marqueeText_t* marquee = (marqueeText_t*)os_malloc(sizeof(marqueeText_t));
    marquee->str[0] = 0;
    switch(evt)
    {
        case EVT_GOT_SICK_RANDOMLY:
        {
            if(!pd->drawSick)
            {
                ets_snprintf(marquee->str, ACT_STRLEN, "%s got sick. ", pd->demon.name);
            }
            break;
        }
        case EVT_GOT_SICK_POOP:
        {
            if(!pd->drawSick)
            {
                ets_snprintf(marquee->str, ACT_STRLEN, "Poop made %s sick. ", pd->demon.name);
            }
            break;
        }
        case EVT_GOT_SICK_OBESE:
        {
            if(!pd->drawSick)
            {
                ets_snprintf(marquee->str, ACT_STRLEN, "Obesity made %s sick. ", pd->demon.name);
            }
            break;
        }
        case EVT_GOT_SICK_MALNOURISHED:
        {
            if(!pd->drawSick)
            {
                ets_snprintf(marquee->str, ACT_STRLEN, "Hunger made %s sick. ", pd->demon.name);
            }
            break;
        }
        case EVT_POOPED:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_POOPING);
            break;
        }
        case EVT_LOST_DISCIPLINE:
        {
            // TODO Animate getting rowdy?
            ets_snprintf(marquee->str, ACT_STRLEN, "%s got rowdy. ", pd->demon.name);
            break;
        }
        case EVT_EAT:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_EATING);
            break;
        }
        case EVT_OVEREAT:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_OVER_EATING);
            break;
        }
        case EVT_NO_EAT_SICK:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_EATING);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is too sick to eat. ", pd->demon.name);
            break;
        }
        case EVT_NO_EAT_DISCIPLINE:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_EATING);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is too rowdy eat. ", pd->demon.name);
            break;
        }
        case EVT_NO_EAT_FULL:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_EATING);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is too full to eat. ", pd->demon.name);
            break;
        }
        case EVT_PLAY:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_PLAYING);
            break;
        }
        case EVT_NO_PLAY_DISCIPLINE:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_NOT_PLAYING);
            break;
        }
        case EVT_SCOLD:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_SCOLD);
            break;
        }
        case EVT_NO_SCOLD_SICK:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_SCOLD);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is sick. ", pd->demon.name);
            break;
        }
        case EVT_MEDICINE_NOT_SICK:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s wasn't sick. ", pd->demon.name);
            break;
        }
        case EVT_MEDICINE_CURE:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is cured. ", pd->demon.name);
            break;
        }
        case EVT_MEDICINE_FAIL:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_MEDICINE);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s isn't cured. ", pd->demon.name);
            break;
        }
        case EVT_FLUSH_POOP:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_FLUSH);
            break;
        }
        case EVT_FLUSH_NOTHING:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_FLUSH);
            break;
        }
        case EVT_LOST_HEALTH_SICK:
        {
            ets_snprintf(marquee->str, ACT_STRLEN, "%s lost health to sickness. ", pd->demon.name);
            break;
        }
        case EVT_LOST_HEALTH_OBESITY:
        {
            ets_snprintf(marquee->str, ACT_STRLEN, "%s lost health to obesity. ", pd->demon.name);
            break;
        }
        case EVT_LOST_HEALTH_MALNOURISHMENT:
        {
            ets_snprintf(marquee->str, ACT_STRLEN, "%s lost health to hunger. ", pd->demon.name);
            break;
        }
        case EVT_TEENAGER:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_BIRTHDAY);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is a teenager. ", pd->demon.name);
            break;
        }
        case EVT_ADULT:
        {
            unshift(&pd->animationQueue, (void*)PDA_CENTER);
            unshift(&pd->animationQueue, (void*)PDA_BIRTHDAY);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s is an adult. ", pd->demon.name);
            break;
        }
        case EVT_BORN:
        {
            unshift(&pd->animationQueue, (void*)PDA_BIRTH);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s arrived. ", pd->demon.name);
            break;
        }
        case EVT_DEAD:
        {
            unshift(&pd->animationQueue, (void*)PDA_DEATH);
            ets_snprintf(marquee->str, ACT_STRLEN, "%s died. ", pd->demon.name);
            break;
        }
        default:
        case EVT_NONE:
        case EVT_NUM_EVENTS:
        {
            os_free(marquee);
            return;
        }
    }

    if(0 != marquee->str[0])
    {
        // If there is no marquee text
        if(pd->marqueeTextQueue.length == 0)
        {
            // Position this at the edge of the OLED
            marquee->pos = OLED_WIDTH;
        }
        else
        {
            // Otherwise position this after the last text
            // Find the last node in the marquee
            node_t* node = pd->marqueeTextQueue.first;
            while(NULL != node->next)
            {
                node = node->next;
            }
            marqueeText_t* lastText = node->val;
            // Set the position
            marquee->pos = lastText->pos + textWidth(lastText->str, IBM_VGA_8);

            // If this would already be on the OLED
            if(marquee->pos < OLED_WIDTH)
            {
                // shift it to the edge
                marquee->pos = OLED_WIDTH;
            }
        }

        push(&pd->marqueeTextQueue, (void*)marquee);
    }
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimWalk(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;
    // Take one step every 320ms
    bool retval = false;
    while (pd->animTimeUs >= 320000)
    {
        pd->animTimeUs -= 320000;
        retval = true;
        // Check if the demon turns around
        if(os_random() % 32 == 0)
        {
            pd->demonDirLR = !(pd->demonDirLR);
        }

        if(pd->demonX >= OLED_WIDTH - pd->demonSprite.width - pd->heart.width)
        {
            pd->demonDirLR = false;
        }

        if(pd->demonX <= 0)
        {
            pd->demonDirLR = true;
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
        if(os_random() % 2 == 0)
        {
            // Check if the demon changes up/down direction
            if(os_random() % 8 == 0)
            {
                pd->demonDirUD = !(pd->demonDirUD);
            }

            if (pd->demonY >= OLED_HEIGHT - pd->demonSprite.height - FONT_HEIGHT_IBMVGA8 - 4)
            {
                pd->demonDirUD = false;
            }
            if (pd->demonY <= FONT_HEIGHT_IBMVGA8 + 1)
            {
                pd->demonDirUD = true;
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
    }
    return retval;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimCenter(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;
    bool retval = false;
    while (pd->animTimeUs >= 40000)
    {
        pd->animTimeUs -= 40000;
        retval = true;
        bool centeredX = false;
        bool centeredY = false;

        if(pd->demonX > (OLED_WIDTH / 2) - (pd->demonSprite.width / 2))
        {
            pd->demonDirLR = false;
            pd->demonX -= 2;
            if(pd->demonX < (OLED_WIDTH / 2) - (pd->demonSprite.width / 2))
            {
                pd->demonX = (OLED_WIDTH / 2) - (pd->demonSprite.width / 2);
            }
        }
        else if(pd->demonX < (OLED_WIDTH / 2) - (pd->demonSprite.width / 2))
        {
            pd->demonDirLR = true;
            pd->demonX += 2;
            if(pd->demonX > (OLED_WIDTH / 2) - (pd->demonSprite.width / 2))
            {
                pd->demonX = (OLED_WIDTH / 2) - (pd->demonSprite.width / 2);
            }
        }
        else
        {
            centeredX = true;
        }

        if(pd->demonY > (OLED_HEIGHT / 2) - (pd->demonSprite.height / 2))
        {
            pd->demonY--;
        }
        else if(pd->demonY < (OLED_HEIGHT / 2) - (pd->demonSprite.height / 2))
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
            break;
        }
    }
    return retval;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimDemon(void)
{
    // Draw the demon
    if(pd->drawFat)
    {
        drawPng((&pd->demonSpriteFat), pd->demonX, pd->demonY, pd->demonDirLR, false, pd->demonRot);
    }
    else if(pd->drawThin)
    {
        drawPng((&pd->demonSpriteThin), pd->demonX, pd->demonY, pd->demonDirLR, false, pd->demonRot);
    }
    else if(pd->drawSick)
    {
        drawPng((&pd->demonSpriteSick), pd->demonX, pd->demonY, pd->demonDirLR, false, pd->demonRot);
    }
    else
    {
        drawPng((&pd->demonSprite), pd->demonX, pd->demonY, pd->demonDirLR, false, pd->demonRot);
    }
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
bool ICACHE_FLASH_ATTR updtAnimEating(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;
    bool shouldDraw = false;

    static bool isCentered = false;
    if(false == isCentered && pd->animTimeUs >= 500000)
    {
        pd->demonX = (OLED_WIDTH / 2) - (pd->demonSprite.width / 2);
        isCentered = true;
        shouldDraw = true;
        if(pd->seqFrame == pd->burger.count)
        {
            personalDemonResetAnimVars();
            isCentered = false;
        }
    }
    else if(true == isCentered && pd->animTimeUs >= 1000000)
    {
        pd->animTimeUs -= 1000000;
        pd->demonX = (OLED_WIDTH / 2) - (3 * (pd->demonSprite.width / 4));
        isCentered = false;
        shouldDraw = true;
        pd->seqFrame++;
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
        drawPngSequence(pd->food,
                        (OLED_WIDTH / 2) - (3 * (pd->demonSprite.width / 4)) - (pd->food->handles->width) - (3),
                        (OLED_HEIGHT / 2) - (pd->food->handles->height / 2),
                        false, false, 0, pd->seqFrame);
    }
    else
    {
        drawPngSequence(pd->food,
                        (OLED_WIDTH / 2) - (3 * (pd->demonSprite.width / 4)) - (pd->food->handles->width) - (2),
                        (OLED_HEIGHT / 2) - (pd->food->handles->height) - 1,
                        false, false, 0, pd->seqFrame);
        drawPngSequence(pd->food,
                        (OLED_WIDTH / 2) - (3 * (pd->demonSprite.width / 4)) - (pd->food->handles->width) - (pd->food->handles->width) - (4),
                        (OLED_HEIGHT / 2) - (pd->food->handles->height) - 1,
                        false, false, 0, pd->seqFrame);
        drawPngSequence(pd->food,
                        (OLED_WIDTH / 2) - (3 * (pd->demonSprite.width / 4)) - (pd->food->handles->width) - (pd->food->handles->width / 2) -
                        (3),
                        (OLED_HEIGHT / 2) + 1,
                        false, false, 0, pd->seqFrame);
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
bool ICACHE_FLASH_ATTR updtAnimNotEating(uint32_t tElapsed)
{
    bool shouldDraw = false;

    pd->animTimeUs += tElapsed;

    static uint8_t turns = 0;
    while (pd->animTimeUs >= 500000)
    {
        pd->animTimeUs -= 500000;
        pd->demonDirLR = !pd->demonDirLR;
        shouldDraw = true;
        turns++;
    }

    if(turns == 6)
    {
        turns = 0;
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
    drawPngSequence(pd->food,
                    (OLED_WIDTH / 2) - (3 * (pd->demonSprite.width / 4)) - (pd->food->handles->width) - (3),
                    (OLED_HEIGHT / 2) - (pd->food->handles->height / 2),
                    false, false, 0, 0);
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
bool ICACHE_FLASH_ATTR updtAnimPoop(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;

    static bool drawnPoop = false;
    if(pd->animTimeUs >= 900000)
    {
        drawnPoop = false;
        personalDemonResetAnimVars();
    }
    else if(pd->animTimeUs >= 450000)
    {
        pd->demonRot = 270 + (90 * (pd->animTimeUs - 450000)) / 450000;
        if(false == drawnPoop)
        {
            drawnPoop = true;
            pd->drawPoopCnt++;
        }
    }
    else
    {
        pd->demonRot = 360 - ((90 * pd->animTimeUs) / 450000);
    }

    if(360 == pd->demonRot)
    {
        pd->demonRot = 0;
    }

    return true;
}

/*******************************************************************************
 * Flushing Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimFlush(void)
{
    pd->flushY = FONT_HEIGHT_IBMVGA8 - 3 * (pd->water.height);
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimFlush(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;
    bool retval = false;
    while(pd->animTimeUs >= 30000)
    {
        pd->animTimeUs -= 30000;
        pd->flushY++;
        retval = true;

        if(pd->flushY >= OLED_HEIGHT)
        {
            if(pd->drawPoopCnt > 0)
            {
                pd->drawPoopCnt--;
            }
            personalDemonResetAnimVars();
            break;
        }
    }
    return retval;
}

/**
 * @brief
 *
 */
void ICACHE_FLASH_ATTR drawAnimFlush(void)
{
    // Draw the flush
    for(uint8_t i = 0; i < 3; i++)
    {
        drawPng(&(pd->water), (OLED_WIDTH / 2) + (pd->demonSprite.width / 2),
                pd->flushY + (i * pd->water.height), false, false, 0);
    }

    // Draw the demon
    drawAnimDemon();
}

/*******************************************************************************
 * Playing Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimPlaying(void)
{
    pd->demonDirLR = false;
    pd->ballX = -pd->ball.width;
    pd->ballY = (OLED_HEIGHT / 2) - ((pd->ball.width / 2) / 2);

    pd->ballVelX = 41; // Pixels per second
    pd->ballVelY = 29;
    pd->handRot = 0;
}

/**
 * @brief
 *
 */
bool ICACHE_FLASH_ATTR updtAnimPlaying(uint32_t tElapsed)
{
    return _updtAnimPlaying(tElapsed, true);
}

/**
 * @brief
 *
 */
bool ICACHE_FLASH_ATTR updtAnimNotPlaying(uint32_t tElapsed)
{
    return _updtAnimPlaying(tElapsed, false);
}

/**
 * @brief
 *
 */
bool ICACHE_FLASH_ATTR _updtAnimPlaying(uint32_t tElapsed, bool isPlaying)
{
    static uint8_t bounces = 0;

    // Figure out how much time elapsed since the last animation
    float deltaS = tElapsed / 1000000.0f;

    // Update ball position and rotation
    pd->handRot += (deltaS * 180);
    if(pd->handRot >= 360)
    {
        pd->handRot -= 360;
    }
    pd->ballX += (deltaS * pd->ballVelX);
    pd->ballY += (deltaS * pd->ballVelY);

    // Bounce the ball off the walls
    if((pd->ballY + (pd->ball.width / 2) >= OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 4) && pd->ballVelY > 0)
    {
        pd->ballVelY = -pd->ballVelY;
    }
    else if((pd->ballY - (pd->ball.width / 2) <= FONT_HEIGHT_IBMVGA8 + 1) && pd->ballVelY < 0)
    {
        pd->ballVelY = -pd->ballVelY;
    }
    else if(isPlaying && (pd->ballX + (pd->ball.width / 2) >= OLED_WIDTH - pd->heart.width) && pd->ballVelX > 0)
    {
        pd->ballVelX = -pd->ballVelX;
    }
    else if(bounces < MAX_BOUNCES && (pd->ballX - (pd->ball.width / 2) < 0) && pd->ballVelX < 0)
    {
        pd->ballVelX = -pd->ballVelX;
    }
    else if(isPlaying && bounces >= MAX_BOUNCES && pd->ballX + pd->ball.width < 0)
    {
        // Bounced enough, time to finish
        bounces = 0;
        personalDemonResetAnimVars();
        return true;
    }
    else if(!isPlaying && pd->ballX - pd->ball.width / 2 > OLED_WIDTH - pd->heart.width)
    {
        // Not playing, and the ball is gone, time to finish
        bounces = 0;
        personalDemonResetAnimVars();
        return true;
    }

    if(isPlaying)
    {
        // Demon tracks ball
        pd->demonY = pd->ballY - (pd->ball.width / 2) - (pd->demonSprite.height / 2);

        // Bounce the ball off the demon
        if((pd->ballY - (pd->ball.width / 2) >= pd->demonY) &&
                (pd->ballY < pd->demonY + pd->demonSprite.height))
        {
            if((pd->ballX + (pd->ball.width / 2) >= pd->demonX) &&
                    (pd->ballX - (pd->ball.width / 2) < pd->demonX) &&
                    pd->ballVelX > 0)
            {
                pd->ballVelX = -pd->ballVelX;
                bounces++;
            }
            else if((pd->ballX + (pd->ball.width / 2) >= pd->demonX + pd->demonSprite.width) &&
                    (pd->ballX - (pd->ball.width / 2) < pd->demonX + pd->demonSprite.width) &&
                    pd->ballVelX < 0)
            {
                pd->ballVelX = -pd->ballVelX;
                bounces++;
            }
        }
    }
    else
    {
        // Demon looks away from ball, indifferent
        if(pd->ballX < OLED_WIDTH / 2)
        {
            pd->demonDirLR = true;
        }
        else
        {
            pd->demonDirLR = false;
        }
    }
    return true;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimPlaying(void)
{
    // Draw the demon
    drawAnimDemon();

    // Draw the ball
    drawPng(&(pd->ball),
            pd->ballX - (pd->ball.width / 2), pd->ballY - (pd->ball.width / 2),
            false, false, pd->handRot);
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
bool ICACHE_FLASH_ATTR updtAnimMeds(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;

    bool retval = false;
    while (pd->animTimeUs >= 100000)
    {
        pd->animTimeUs -= 100000;
        retval = true;

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
            break;
        }
    }
    return retval;
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
    drawPngSequence(&(pd->syringe),
                    (OLED_WIDTH / 2) - (pd->demonSprite.width / 2) - pd->syringe.handles->width - 2,
                    (OLED_HEIGHT / 2) - (pd->syringe.handles->height / 2),
                    false, false, 0, pd->seqFrame);
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
bool ICACHE_FLASH_ATTR updtAnimScold(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;
    static int16_t animCnt = 0;

    bool drewUpdate = false;
    while (pd->animTimeUs > 10000)
    {
        pd->animTimeUs -= 10000;
        animCnt++;

        if(animCnt >= 180)
        {
            personalDemonResetAnimVars();
            animCnt = 0;
        }
        else if(animCnt >= 135)
        {
            pd->handRot -= 2;
        }
        else if(animCnt >= 90)
        {
            if(animCnt == 125)
            {
                pd->demonDirLR = !pd->demonDirLR;
            }
            pd->handRot += 2;
        }
        else if(animCnt >= 45)
        {
            if(animCnt == 45)
            {
                pd->demonY += 1;
            }
            pd->handRot -= 2;
        }
        else
        {
            pd->handRot += 2;
        }
        drewUpdate = true;
    }
    return drewUpdate;
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
    drawPng((&pd->hand),
            (OLED_WIDTH / 2) - (pd->demonSprite.width / 2) - pd->hand.width - 4,
            FONT_HEIGHT_IBMVGA8,
            false, false, pd->handRot);
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
    pd->demonY = 11 + 38 - pd->demonSprite.height;
    pd->demonX = 16 + 24 - pd->demonSprite.width;
}

/**
 * @brief TODO
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimPortal(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;
    bool retval = false;
    while(pd->animTimeUs >= 80000)
    {
        retval  = true;
        pd->animTimeUs -= 80000;
        pd->demonX++;
        if(pd->demonX % 2 == 1)
        {
            pd->demonY--;
        }
        else
        {
            pd->demonY++;
        }

        if(pd->demonX > 16 + pd->archL.width + pd->archR.width)
        {
            personalDemonResetAnimVars();
            break;
        }
    }
    return retval;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimPortal(void)
{
    // Draw half of the portal
    drawPng(&pd->archR, 16 + pd->archL.width, 11, false, false, 0);
    // Draw the demon
    drawAnimDemon();
    // Cover the area peeking out of the portal
    fillDisplayArea(0, 0, 16, OLED_HEIGHT, BLACK);
    // Draw the other half
    drawPng(&pd->archL, 16, 11, false, false, 0);
}

/*******************************************************************************
 * Death Animation
 ******************************************************************************/

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR initAnimDeath(void)
{
    pd->demonDirLR = false;
    pd->demonRot = 0;
    // Save demon record
    addDemonMemorial(pd->demon.name, pd->demon.actionsTaken);
}

/**
 * @brief TODO every 5 ms
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimDeath(uint32_t tElapsed)
{
    pd->animTimeUs += tElapsed;

    static uint8_t animPhase = 0;
    switch (animPhase)
    {
        case 0:
        {
            // Rotate backwards
            if(pd->animTimeUs < 900000)
            {
                pd->demonRot = (90 * pd->animTimeUs) / 900000;
            }
            else
            {
                pd->animTimeUs = 0;
                animPhase++;
            }
            break;
        }
        case 1:
        {
            // Fall down
            if(pd->demonY < OLED_HEIGHT)
            {
                while (pd->animTimeUs > 40000)
                {
                    pd->animTimeUs -= 40000;
                    pd->demonY++;
                }
            }
            else
            {
                pd->drawPoopCnt = 0;
                pd->animTimeUs = 0;
                animPhase++;
            }
            break;
        }
        default:
        case 2:
        {
            // Display text
            if(pd->animTimeUs > 5000000) // 5 seconds
            {
                animPhase = 0;

                personalDemonResetAnimVars();

                // The demon is dead, so make a new one. Reset the demon
                resetDemon(&(pd->demon));
                // And immediately save it
                setSavedDemon(&(pd->demon));

                // Reload the PNGs for the new demon
                freePngAsset(&(pd->demonSprite));
                freePngAsset(&(pd->demonSpriteFat));
                freePngAsset(&(pd->demonSpriteThin));
                freePngAsset(&(pd->demonSpriteSick));
                allocPngAsset(demonSprites[pd->demon.species].norm, &(pd->demonSprite));
                allocPngAsset(demonSprites[pd->demon.species].fat,  &(pd->demonSpriteFat));
                allocPngAsset(demonSprites[pd->demon.species].thin, &(pd->demonSpriteThin));
                allocPngAsset(demonSprites[pd->demon.species].sick, &(pd->demonSpriteSick));

                // Initialize demon draw state
                pd->drawSick = pd->demon.isSick;
                pd->drawFat = isDemonObese(&(pd->demon));
                pd->drawThin = isDemonThin(&(pd->demon));
                pd->drawHealth = pd->demon.health;
                pd->drawPoopCnt = pd->demon.poopCount;
                pd->ledHappy = CLAMP(pd->demon.happy, -4, 4);
                pd->ledDiscipline = CLAMP(pd->demon.discipline, -4, 4);

                // Clear the queues
                ets_memset(&(pd->demon.evQueue), EVT_NONE, sizeof(pd->demon.evQueue));

                while(pd->marqueeTextQueue.length > 0)
                {
                    void* node = pop(&(pd->marqueeTextQueue));
                    os_free(node);
                }

                while(pd->animationQueue.length > 0)
                {
                    pop(&(pd->animationQueue));
                }

                // Start
                animateEvent(EVT_BORN);
            }
            break;
        }
    }

    return true;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimDeath(void)
{
    if(pd->demonY < OLED_HEIGHT)
    {
        // Draw the demon
        drawAnimDemon();
    }
    else
    {
        char str[64] = {0};

        ets_snprintf(str, sizeof(str), "%s", pd->demon.name);
        int16_t width = textWidth(str, IBM_VGA_8);
        plotText((OLED_WIDTH - width) / 2, OLED_HEIGHT / 2 - FONT_HEIGHT_IBMVGA8 - 1,
                 str, IBM_VGA_8, WHITE);

        ets_snprintf(str, sizeof(str), "lived %d days", pd->demon.actionsTaken);
        width = textWidth(str, IBM_VGA_8);
        plotText((OLED_WIDTH - width) / 2, OLED_HEIGHT / 2 + 1,
                 str, IBM_VGA_8, WHITE);
    }
}

/*******************************************************************************
 * Birthday Animation
 ******************************************************************************/

/**
 * @brief TODO every 5 ms
 *
 * @return true
 * @return false
 */
bool ICACHE_FLASH_ATTR updtAnimBirthday(uint32_t tElapsed)
{
    bool shouldDraw = false;

    pd->animTimeUs += tElapsed;

    static bool isUp = true;
    static uint8_t jumps = 0;

    if(true == isUp && pd->animTimeUs >= 500000)
    {
        pd->demonY -= 6;
        isUp = false;
        shouldDraw = true;
        jumps++;
    }
    else if(false == isUp && pd->animTimeUs >= 1000000)
    {
        pd->animTimeUs -= 1000000;
        pd->demonY += 6;
        isUp = true;
        shouldDraw = true;
    }

    if(4 == jumps)
    {
        personalDemonResetAnimVars();
        shouldDraw = true;
        isUp = true;
        jumps = 0;
    }
    return shouldDraw;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR drawAnimBirthday(void)
{
    // Draw the demon
    drawAnimDemon();
    drawPng(&(pd->cake), pd->demonX - pd->cake.width - 4, (OLED_HEIGHT - pd->cake.height) / 2, false, false, 0);
}
