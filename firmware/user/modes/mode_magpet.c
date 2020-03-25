/*==============================================================================
 * Includes
 *============================================================================*/

#include "mode_magpet.h"
#include "p2pConnection.h"
#include "buttons.h"
#include "bresenham.h"
#include "font.h"
#include "assets.h"
#include "buzzer.h"
#include "hpatimer.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define magpetPrintf(...) do { \
        os_snprintf(lastMsg, sizeof(lastMsg), __VA_ARGS__); \
        os_printf("%s", lastMsg); \
        magpetUpdateDisplay(); \
    } while(0)

#define ID_NUM "idn"
#define lengthof(a) (sizeof(a) / sizeof(a[0]))

/*==============================================================================
 * Function Prototypes
 *============================================================================*/

void magpetEnterMode(void);
void magpetExitMode(void);
void magpetButtonCallback(uint8_t state, int button, int down);
void magpetEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len,
                        uint8_t rssi);
void magpetEspNowSendCb(uint8_t* mac_addr, mt_tx_status status);

void magpetConCbFn(p2pInfo* p2p, connectionEvt_t);
void magpetMsgRxCbFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void magpetMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);
void sendWhoAmI(p2pInfo* p2p);

void magpetUpdateDisplay(void);
void magpetAnimationTimer(void* arg __attribute__((unused)));
void resetTheirPet(bool);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode magpetMode =
{
    .modeName = "magpet",
    .fnEnterMode = magpetEnterMode,
    .fnExitMode = magpetExitMode,
    .fnButtonCallback = magpetButtonCallback,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = magpetEspNowRecvCb,
    .fnEspNowSendCb = magpetEspNowSendCb,
    .fnAccelerometerCallback = NULL
};

p2pInfo connection;
char lastMsg[256] = {0};
syncedTimer_t animationTimer;

uint8_t myPet = 0;
int8_t myPetOffset = 0;

uint8_t theirPet = 0xFF;
int16_t theirPetOffset = -1;
bool theirPetMovingLeft = true;

const char petSprites[][16] =
{
    "baby_dino.png",
    "blacky.png",
    "coldhead.png",
    "dino.png",
    "horn_dino.png",
    "serpent.png",
    "turd.png",
    "bear.png",
    "blob.png",
    "crouch.png",
    "dragon.png",
    "hothead.png",
    "slug.png",
    "wing_snake.png"
};

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initialize the magpet mode
 */
void ICACHE_FLASH_ATTR magpetEnterMode(void)
{
    // Set up a connection
    ets_memset(&connection, 0, sizeof(connection));
    p2pInitialize(&connection, "pet", magpetConCbFn, magpetMsgRxCbFn, 0);

    // Pick a true random pet
    while((myPet = (os_random() & 0x0F)) >= lengthof(petSprites));

    // Clear their pet data
    resetTheirPet(false);

    // Set up an animation timer
    syncedTimerSetFn(&animationTimer, magpetAnimationTimer, NULL);
    syncedTimerArm(&animationTimer, 50, true);

    // Draw the initial display
    magpetUpdateDisplay();
}

/**
 * De-initialize the magpet mode
 */
void ICACHE_FLASH_ATTR magpetExitMode(void)
{
    p2pDeinit(&connection);
}

/**
 * Magpet mode button press handler. Either start connections or send messages
 * depending on the current state
 *
 * @param state  A bitmask of all buttons currently
 * @param button The button that changed state
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR magpetButtonCallback(uint8_t state __attribute__((unused)),
        int button, int down)
{
    // If it was pressed
    if(down)
    {
        switch(button)
        {
            default:
            case 0:
            {
                // Shouldn't be able to get here
                break;
            }
            case 1: // Left
            {
                if(!connection.cnc.isConnected)
                {
                    p2pStartConnection(&connection);
                }
                break;
            }
            case 2: // Right
            {
                if(connection.cnc.isConnected)
                {
                    // Otherwise send a message
                    // p2pSendMsg(&connection, PET_LABEL,
                    //            "TST_MSG", sizeof("TST_MSG"), magpetMsgTxCbFn);
                }
            }
            break;
        }
    }
    magpetUpdateDisplay();
}

/**
 * Callback function when ESP-NOW receives a packet. Forward everything to all
 * p2p connections and let them handle it
 *
 * @param mac_addr The MAC of the swadge that sent the data
 * @param data     The data
 * @param len      The length of the data
 * @param rssi     The RSSI of th received message, a proxy for distance
 */
void ICACHE_FLASH_ATTR magpetEspNowRecvCb(uint8_t* mac_addr, uint8_t* data,
        uint8_t len, uint8_t rssi)
{
    p2pRecvCb(&connection, mac_addr, data, len, rssi);
    magpetUpdateDisplay();
}

/**
 * Callback function when ESP-NOW sends a packet. Forward everything to all p2p
 * connections and let them handle it
 *
 * @param mac_addr unused
 * @param status   Whether the transmission succeeded or failed
 */
void ICACHE_FLASH_ATTR magpetEspNowSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    os_printf("%s::%d\n", __func__, __LINE__);
    p2pSendCb(&connection, mac_addr, status);
    magpetUpdateDisplay();
}

/**
 * Callback function when p2p connection events occur. Whenever a connection
 * starts, halt all the other p2ps from connecting.
 *
 * @param p2p The p2p struct which emitted a connection event
 * @param evt The connection event
 */
void ICACHE_FLASH_ATTR magpetConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    switch(evt)
    {
        case CON_STARTED:
        {
            magpetPrintf("CON_STARTED\n");
            break;
        }
        case CON_STOPPED:
        {
            magpetPrintf("CON_STOPPED\n");
            resetTheirPet(false);
            break;
        }
        case RX_BROADCAST:
        {
            magpetPrintf("RX_BROADCAST\n");
            break;
        }
        case RX_GAME_START_ACK:
        {
            magpetPrintf("RX_GAME_START_ACK\n");
            break;
        }
        case RX_GAME_START_MSG:
        {
            magpetPrintf("RX_GAME_START_MSG\n");
            break;
        }
        case CON_ESTABLISHED:
        {
            magpetPrintf("CON_ESTABLISHED\n");
            // After connection, the first player sends their info
            if(GOING_FIRST == p2p->cnc.playOrder)
            {
                sendWhoAmI(p2p);
            }
            break;
        }
        case CON_LOST:
        {
            magpetPrintf("CON_LOST\n");
            resetTheirPet(false);
            break;
        }
        default:
        {
            magpetPrintf("Unknown event %d\n", evt);
            break;
        }
    }
    magpetUpdateDisplay();
    return;
}

/**
 * Callback function when p2p receives a message. Draw a little animation if
 * the message is correct
 *
 * @param p2p     The p2p struct which received a message
 * @param msg     The label for the received message
 * @param payload The payload for the received message
 * @param len     The length of the payload
 */
void ICACHE_FLASH_ATTR magpetMsgRxCbFn(p2pInfo* p2p __attribute__((unused)),
                                       char* msg,
                                       uint8_t* payload __attribute__((unused)),
                                       uint8_t len)
{
    // This receives the other swadge's pet info
    if(len == 1 && 0 == ets_strcmp(msg, ID_NUM))
    {
        // If another pet isn't registered yet
        if(0xFF == theirPet)
        {
            // Save their pet and start animating
            theirPet = payload[0] - '0';
            theirPetOffset = OLED_WIDTH;

            // Send our pet info back
            sendWhoAmI(p2p);

            // And play a little jingle
            uint32_t songLen;
            startBuzzerSong((song_t*)getAsset("friends.rtl", &songLen), false);
        }
    }
    magpetUpdateDisplay();
}

/**
 * Callback function when p2p sends a message. If the message failed, treat
 * that side as disconnected
 *
 * @param p2p    The p2p struct which sent a message
 * @param status The status of the transmission
 */
void ICACHE_FLASH_ATTR magpetMsgTxCbFn(p2pInfo* p2p __attribute__((unused)),
                                       messageStatus_t status)
{
    switch(status)
    {
        case MSG_ACKED:
        {
            magpetPrintf("MSG_ACKED\n");
            break;
        }
        case MSG_FAILED:
        {
            magpetPrintf("MSG_FAILED\n");
            resetTheirPet(true);
            break;
        }
        default:
        {
            magpetPrintf("Unknown status %d\n", status);
            break;
        }
    }
    magpetUpdateDisplay();
    return;
}

/**
 * Helper function to send our pet's info to the connected swadge
 *
 * @param p2p The connection to send info through
 */
void ICACHE_FLASH_ATTR sendWhoAmI(p2pInfo* p2p)
{
    char myPetStr[] = {'0' + myPet, 0};
    p2pSendMsg(p2p, ID_NUM, myPetStr, strlen(myPetStr), magpetMsgTxCbFn);
}

/**
 * Update the OLED
 */
void ICACHE_FLASH_ATTR magpetUpdateDisplay(void)
{
    // Clear everything
    clearDisplay();

    // Draw the last debug text to the OLED
    plotText(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1, lastMsg, IBM_VGA_8, WHITE);

    // Draw our pet
    drawBitmapFromAsset(petSprites[myPet], OLED_WIDTH / 4, OLED_HEIGHT / 2 - 8 + myPetOffset, false, false, 0);

    // Draw their pet, maybe
    if(0xFF != theirPet && theirPetOffset > 0)
    {
        drawBitmapFromAsset(petSprites[theirPet], theirPetOffset, OLED_HEIGHT / 2 - 8, false, false, 0);
    }

    switch(connection.cnc.playOrder)
    {
        case GOING_FIRST:
            plotText(0, OLED_HEIGHT - (2 * FONT_HEIGHT_IBMVGA8) - 2, "First", IBM_VGA_8, WHITE);
            break;
        case GOING_SECOND:
            plotText(0, OLED_HEIGHT - (2 * FONT_HEIGHT_IBMVGA8) - 2, "Second", IBM_VGA_8, WHITE);
            break;
        default:
        case NOT_SET:
            break;
    }
}

/**
 * Timer function for animation
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR magpetAnimationTimer(void* arg __attribute__((unused)))
{
    static uint8_t frames = 0;
    frames++;
    if(frames == 8)
    {
        frames = 0;
        myPetOffset = (myPetOffset > 0) ? -2 : 2;
    }

    // If there's another pet to draw
    if(0xFF != theirPet)
    {
        // Move it left or right
        if(theirPetMovingLeft)
        {
            theirPetOffset--;
            // If it moved all the way to the left
            if(theirPetOffset == OLED_WIDTH / 2)
            {
                // Start moving to the right
                theirPetMovingLeft = false;
            }
        }
        else
        {
            theirPetOffset++;
            // If it moved all the way to the right
            if(theirPetOffset == OLED_WIDTH)
            {
                // Reset everything
                resetTheirPet(true);
            }
        }
    }
    magpetUpdateDisplay();
}

/**
 * Helper function to reset their pet and the connection
 *
 * @param restartP2P true to restart the conneciton as well as animation data
 */
void ICACHE_FLASH_ATTR resetTheirPet(bool restartP2P)
{
    theirPet = 0xFF;
    theirPetOffset = -1;
    theirPetMovingLeft = true;
    stopBuzzerSong();
    if(restartP2P)
    {
        p2pRestart(&connection);
    }
}
