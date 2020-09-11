/*==============================================================================
 * Includes
 *============================================================================*/

#include "mode_ring.h"
#include "p2pConnection.h"
#include "buttons.h"
#include "bresenham.h"
#include "font.h"
#include "printControl.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define ringPrintf(...) do { \
        ets_snprintf(lastMsg, sizeof(lastMsg), __VA_ARGS__); \
        RING_PRINTF("%s", lastMsg); \
        ringUpdateDisplay(); \
    } while(0)

#define TST_MSG   "Test Message"
#define TST_LABEL "tst"

#define lengthof(a) (sizeof(a) / sizeof(a[0]))

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    p2pInfo p2p;
    button_mask side;
    char lbl[4];
} ringCon_t;

/*==============================================================================
 * Function Prototypes
 *============================================================================*/

void ringEnterMode(void);
void ringExitMode(void);
void ringButtonCallback(uint8_t state, int button, int down);
void ringEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len,
                      uint8_t rssi);
void ringEspNowSendCb(uint8_t* mac_addr, mt_tx_status status);

void ringConCbFn(p2pInfo* p2p, connectionEvt_t);
void ringMsgRxCbFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void ringMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

void ringUpdateDisplay(void);
void ringAnimationTimer(void* arg __attribute__((unused)));

ringCon_t* getSideConnection(button_mask side);
ringCon_t* getRingConnection(p2pInfo* p2p);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode ringMode =
{
    .modeName = "ring",
    .fnEnterMode = ringEnterMode,
    .fnExitMode = ringExitMode,
    .fnButtonCallback = ringButtonCallback,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = ringEspNowRecvCb,
    .fnEspNowSendCb = ringEspNowSendCb,
    .fnAccelerometerCallback = NULL
};

ringCon_t connections[3];

button_mask connectionSide;

char lastMsg[256];

syncedTimer_t animationTimer;
uint8_t radiusLeft = 0;
uint8_t radiusRight = 0;

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initialize the ring mode
 */
void ICACHE_FLASH_ATTR ringEnterMode(void)
{
    // Clear everything out
    ets_memset(&connections, 0, sizeof(connections));

    // Set the connection labels
    ets_memcpy(connections[0].lbl, "cn0", 3);
    ets_memcpy(connections[1].lbl, "cn1", 3);
    ets_memcpy(connections[2].lbl, "cn2", 3);

    // For each connection, initialize it
    uint8_t i;
    for(i = 0; i < lengthof(connections); i++)
    {
        connections[i].side = 0xFF;
        p2pInitialize(&connections[i].p2p, connections[i].lbl, ringConCbFn,
                      ringMsgRxCbFn, 0);
    }

    // Set up an animation timer
    syncedTimerSetFn(&animationTimer, ringAnimationTimer, NULL);
    syncedTimerArm(&animationTimer, 50, true);

    // Draw the initial display
    ringUpdateDisplay();
}

/**
 * De-initialize the ring mode
 */
void ICACHE_FLASH_ATTR ringExitMode(void)
{
    // For each connection, deinitialize it
    uint8_t i;
    for(i = 0; i < lengthof(connections); i++)
    {
        p2pDeinit(&connections[i].p2p);
    }
}

/**
 * Ring mode button press handler. Either start connections or send messages
 * depending on the current state
 *
 * @param state  A bitmask of all buttons currently
 * @param button The button that changed state
 * @param down   true if the button was pressed, false if it was released
 */
void ICACHE_FLASH_ATTR ringButtonCallback(uint8_t state __attribute__((unused)),
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
            case 2: // Right
            {
                // Save which button was pressed
                button_mask side = (button == 1) ? LEFT : RIGHT;

                // If no one's connected on this side
                if(NULL == getSideConnection(side))
                {
                    // Start connections for all unconnected p2ps
                    connectionSide = side;
                    uint8_t i;
                    for(i = 0; i < lengthof(connections); i++)
                    {
                        if(0xFF == connections[i].side)
                        {
                            p2pStartConnection(&(connections[i].p2p));
                        }
                    }
                }
                else
                {
                    // Otherwise send a message
                    p2pSendMsg(&(getSideConnection(side)->p2p), TST_LABEL,
                               TST_MSG, sizeof(TST_MSG), ringMsgTxCbFn);
                }
                break;
            }
        }
    }
    ringUpdateDisplay();
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
void ICACHE_FLASH_ATTR ringEspNowRecvCb(uint8_t* mac_addr, uint8_t* data,
                                        uint8_t len, uint8_t rssi)
{
    uint8_t i;
    for(i = 0; i < lengthof(connections); i++)
    {
        p2pRecvCb(&(connections[i].p2p), mac_addr, data, len, rssi);
    }
    ringUpdateDisplay();
}

/**
 * Callback function when ESP-NOW sends a packet. Forward everything to all p2p
 * connections and let them handle it
 *
 * @param mac_addr unused
 * @param status   Whether the transmission succeeded or failed
 */
void ICACHE_FLASH_ATTR ringEspNowSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    uint8_t i;
    for(i = 0; i < lengthof(connections); i++)
    {
        p2pSendCb(&(connections[i].p2p), mac_addr, status);
    }
    ringUpdateDisplay();
}

/**
 * Callback function when p2p connection events occur. Whenever a connection
 * starts, halt all the other p2ps from connecting.
 *
 * @param p2p The p2p struct which emitted a connection event
 * @param evt The connection event
 */
void ICACHE_FLASH_ATTR ringConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    // Get the label for debugging
    char* conStr = getRingConnection(p2p)->lbl;

    switch(evt)
    {
        case CON_STARTED:
        {
            ringPrintf("%s: CON_STARTED\n", conStr);
            break;
        }
        case CON_STOPPED:
        {
            ringPrintf("%s: CON_STOPPED\n", conStr);
            break;
        }
        case RX_BROADCAST:
        case RX_GAME_START_ACK:
        case RX_GAME_START_MSG:
        {
            // As soon as one connection starts, stop the others
            uint8_t i;
            for(i = 0; i < lengthof(connections); i++)
            {
                if(p2p != &connections[i].p2p)
                {
                    p2pStopConnection(&connections[i].p2p);
                }
            }
            ringPrintf("%s: %s\n", conStr,
                       (evt == RX_BROADCAST) ? "RX_BROADCAST" :
                       ((evt == RX_GAME_START_ACK) ? "RX_GAME_START_ACK" :
                        "RX_GAME_START_MSG") );
            break;
        }
        case CON_ESTABLISHED:
        {
            // When a connection is established, save the current side to that
            // connection
            getRingConnection(p2p)->side = connectionSide;
            ringPrintf("%s: CON_ESTABLISHED\n", conStr);
            break;
        }
        case CON_LOST:
        {
            // When a connection is lost, clear that side
            getRingConnection(p2p)->side = 0xFF;
            ringPrintf("%s: CON_LOST\n", conStr);
            break;
        }
        default:
        {
            ringPrintf("%s: Unknown event %d\n", conStr, evt);
            break;
        }
    }
    ringUpdateDisplay();
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
void ICACHE_FLASH_ATTR ringMsgRxCbFn(p2pInfo* p2p, char* msg,
                                     uint8_t* payload __attribute__((unused)),
                                     uint8_t len)
{
    if(0 == ets_strcmp(msg, TST_LABEL))
    {
        if(RIGHT == getRingConnection(p2p)->side)
        {
            radiusRight = 1;
        }
        else if(LEFT == getRingConnection(p2p)->side)
        {
            radiusLeft = 1;
        }
    }

    ringPrintf("Received %d bytes from %s\n", len, getRingConnection(p2p)->lbl);
    ringUpdateDisplay();
    return;
}

/**
 * Callback function when p2p sends a message. If the message failed, treat
 * that side as disconnected
 *
 * @param p2p    The p2p struct which sent a message
 * @param status The status of the transmission
 */
void ICACHE_FLASH_ATTR ringMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    // Get the label for debugging
    char* conStr = getRingConnection(p2p)->lbl;

    switch(status)
    {
        case MSG_ACKED:
        {
            // Message acked, do nothing
            ringPrintf("%s: MSG_ACKED\n", conStr);
            break;
        }
        case MSG_FAILED:
        {
            // Message failed, disconnect that side
            ringPrintf("%s: MSG_FAILED\n", conStr);
            getRingConnection(p2p)->side = 0xFF;
            break;
        }
        default:
        {
            ringPrintf("%s: Unknown status %d\n", conStr, status);
            break;
        }
    }
    ringUpdateDisplay();
    return;
}

/**
 * Given a side, left or right, return the connection for that side. If there is
 * no connection, return NULL
 *
 * @param side The side to return a connection for, LEFT or RIGHT
 * @return A pointer to the connection if it exists, or NULL
 */
ringCon_t* ICACHE_FLASH_ATTR getSideConnection(button_mask side)
{
    // For each connection
    uint8_t i;
    for(i = 0; i < lengthof(connections); i++)
    {
        // If the side matches
        if(side == connections[i].side)
        {
            // Return it
            return &connections[i];
        }
    }
    // No connections found
    return NULL;
}

/**
 * Given a p2p struct pointer, return the connection for that pointer. If there
 * is no connection, return NULL
 *
 * @param p2p The p2p to find a ringCon_t for
 * @return A pointer to the connection if it exists, or NULL
 */
ringCon_t* ICACHE_FLASH_ATTR getRingConnection(p2pInfo* p2p)
{
    // For each connection
    uint8_t i;
    for(i = 0; i < lengthof(connections); i++)
    {
        // If the p2p pointer matches
        if(p2p == &connections[i].p2p)
        {
            // Return it
            return &connections[i];
        }
    }
    // No connections found
    return NULL;
}

/**
 * Update the OLED
 */
void ICACHE_FLASH_ATTR ringUpdateDisplay(void)
{
    // Clear everything
    clearDisplay();

    // Draw the last debug text to the OLED
    plotText(6, 0, lastMsg, IBM_VGA_8, WHITE);

    // If either side is connected, draw a rectangle on that side
    if(NULL != getSideConnection(RIGHT))
    {
        plotRect(OLED_WIDTH - 5, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
    }
    if(NULL != getSideConnection(LEFT))
    {
        plotRect(0, 0, 4, OLED_HEIGHT - 1, WHITE);
    }

    // If either circle has a nonzero radius, draw it
    if(radiusRight > 0)
    {
        plotCircle(OLED_WIDTH - 1 - 20, OLED_HEIGHT / 2, radiusRight, WHITE);
    }
    if(radiusLeft > 0)
    {
        plotCircle(20, OLED_HEIGHT / 2, radiusLeft, WHITE);
    }
}

/**
 * Timer function for animation
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR ringAnimationTimer(void* arg __attribute__((unused)))
{
    // Keep track if we should update the OLED
    bool shouldUpdate = false;

    // If the radius is nonzero
    if(radiusLeft > 0)
    {
        // Make the circle bigger
        radiusLeft++;
        // If the radius is 20px
        if(radiusLeft == 20)
        {
            // Stop drawing the circle
            radiusLeft = 0;
        }
        // The OLED should be updated
        shouldUpdate = true;
    }

    // If the radius is nonzero
    if(radiusRight > 0)
    {
        // Make the circle bigger
        radiusRight++;
        // If the radius is 20px
        if(radiusRight == 20)
        {
            // Stop drawing the circle
            radiusRight = 0;
        }
        // The OLED should be updated
        shouldUpdate = true;
    }

    // If there were any changes
    if(shouldUpdate)
    {
        // Update the OLED
        ringUpdateDisplay();
    }
}
