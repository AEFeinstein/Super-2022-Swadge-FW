#include "mode_ring.h"
#include "p2pConnection.h"
#include "buttons.h"
#include "bresenham.h"
#include "font.h"

typedef struct
{
    p2pInfo p2p;
    button_mask side;
    char lbl[4];
} ringCon_t;

void ringEnterMode(void);
void ringExitMode(void);
void ringButtonCallback(uint8_t state, int button, int down);
void ringEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
void ringEspNowSendCb(uint8_t* mac_addr, mt_tx_status status);
void ringAccelerometerCallback(accel_t* accel);

void ringConCbFn(p2pInfo* p2p, connectionEvt_t);
void ringMsgRxCbFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len);
void ringMsgTxCbFn(p2pInfo* p2p, messageStatus_t status);

void ringUpdateDisplay(void);
void ringAnimationTimer(void* arg __attribute__((unused)));

ringCon_t* ICACHE_FLASH_ATTR getSideConnection(button_mask side);
ringCon_t* ICACHE_FLASH_ATTR getRingConnection(p2pInfo* p2p);

swadgeMode ringMode =
{
    .modeName = "ring",
    .fnEnterMode = ringEnterMode,
    .fnExitMode = ringExitMode,
    .fnButtonCallback = ringButtonCallback,
    .wifiMode = ESP_NOW,
    .fnEspNowRecvCb = ringEspNowRecvCb,
    .fnEspNowSendCb = ringEspNowSendCb,
    .fnAccelerometerCallback = ringAccelerometerCallback
};


ringCon_t connections[3];
#define NUM_CONNECTIONS (sizeof(connections) / sizeof(connections[0]))

button_mask connectionSide;

char lastMsg[256];

os_timer_t animationTimer;
uint8_t radiusLeft = 0;
uint8_t radiusRight = 0;

ringCon_t* ICACHE_FLASH_ATTR getSideConnection(button_mask side)
{
    uint8_t i;
    for(i = 0; i < NUM_CONNECTIONS; i++)
    {
        if(side == connections[i].side)
        {
            return &connections[i];
        }
    }
    return NULL;
}

ringCon_t* ICACHE_FLASH_ATTR getRingConnection(p2pInfo* p2p)
{
    uint8_t i;
    for(i = 0; i < NUM_CONNECTIONS; i++)
    {
        if(p2p == &connections[i].p2p)
        {
            return &connections[i];
        }
    }
    return NULL;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR ringEnterMode(void)
{
    ets_memset(&connections, 0, sizeof(connections));

    memcpy(connections[0].lbl, "cn0", 3);
    memcpy(connections[1].lbl, "cn1", 3);
    memcpy(connections[2].lbl, "cn2", 3);

    uint8_t i;
    for(i = 0; i < NUM_CONNECTIONS; i++)
    {
        connections[i].side = 0xFF;
        p2pInitialize(&connections[i].p2p, connections[i].lbl, ringConCbFn, ringMsgRxCbFn, 0);
    }

    os_timer_setfn(&animationTimer, ringAnimationTimer, NULL);
    os_timer_arm(&animationTimer, 50, true);

    ringUpdateDisplay();
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR ringExitMode(void)
{
    uint8_t i;
    for(i = 0; i < NUM_CONNECTIONS; i++)
    {
        p2pDeinit(&connections[i].p2p);
    }
}

/**
 * @brief TODO
 *
 * @param state
 * @param button
 * @param down
 */
void ICACHE_FLASH_ATTR ringButtonCallback(uint8_t state __attribute__((unused)), int button, int down)
{
    if(down)
    {
        switch(button)
        {
            default:
            case 0:
            {
                break;
            }
            case 1: // Left
            case 2: // Right
            {
                button_mask side = (button == 1) ? LEFT : RIGHT;

                // If no one's connected on this side
                if(NULL == getSideConnection(side))
                {
                    // Start connections for unconnected p2ps
                    connectionSide = side;
                    uint8_t i;
                    for(i = 0; i < NUM_CONNECTIONS; i++)
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
                    p2pSendMsg(&(getSideConnection(side)->p2p), "tst", "Tst Msg", sizeof("Tst Msg"), ringMsgTxCbFn);
                }
                break;
            }
        }
    }
    ringUpdateDisplay();
}

/**
 * @brief TODO
 *
 * @param mac_addr
 * @param data
 * @param len
 * @param rssi
 */
void ICACHE_FLASH_ATTR ringEspNowRecvCb(uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi)
{
    uint8_t i;
    for(i = 0; i < NUM_CONNECTIONS; i++)
    {
        p2pRecvCb(&(connections[i].p2p), mac_addr, data, len, rssi);
    }
    ringUpdateDisplay();
}

/**
 * @brief TODO
 *
 * @param mac_addr
 * @param status
 */
void ICACHE_FLASH_ATTR ringEspNowSendCb(uint8_t* mac_addr, mt_tx_status status)
{
    uint8_t i;
    for(i = 0; i < NUM_CONNECTIONS; i++)
    {
        p2pSendCb(&(connections[i].p2p), mac_addr, status);
    }
    ringUpdateDisplay();
}

/**
 * @brief TODO
 *
 * @param accel
 */
void ICACHE_FLASH_ATTR ringAccelerometerCallback(accel_t* accel __attribute__((unused)))
{
    ringUpdateDisplay();
    return;
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param evt
 */
void ICACHE_FLASH_ATTR ringConCbFn(p2pInfo* p2p, connectionEvt_t evt)
{
    char* conStr = getRingConnection(p2p)->lbl;

    switch(evt)
    {
        case CON_STARTED:
        {
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: CON_STARTED\n", conStr);
            break;
        }
        case CON_STOPPED:
        {
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: CON_STOPPED\n", conStr);
            break;
        }
        case RX_GAME_START_ACK:
        {
            uint8_t i;
            for(i = 0; i < NUM_CONNECTIONS; i++)
            {
                if(p2p != &connections[i].p2p)
                {
                    p2pStopConnection(&connections[i].p2p);
                }
            }

            os_snprintf(lastMsg, sizeof(lastMsg), "%s: RX_GAME_START_ACK\n", conStr);
            break;
        }
        case RX_GAME_START_MSG:
        {
            uint8_t i;
            for(i = 0; i < NUM_CONNECTIONS; i++)
            {
                if(p2p != &connections[i].p2p)
                {
                    p2pStopConnection(&connections[i].p2p);
                }
            }

            os_snprintf(lastMsg, sizeof(lastMsg), "%s: RX_GAME_START_MSG\n", conStr);
            break;
        }
        case CON_ESTABLISHED:
        {
            getRingConnection(p2p)->side = connectionSide;
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: CON_ESTABLISHED\n", conStr);
            break;
        }
        case CON_LOST:
        {
            getRingConnection(p2p)->side = 0xFF;
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: CON_LOST\n", conStr);
            break;
        }
        default:
        {
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: Unknown event %d\n", conStr, evt);
            break;
        }
    }
    os_printf("%s", lastMsg);
    ringUpdateDisplay();
    return;
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param msg
 * @param payload
 * @param len
 */
void ICACHE_FLASH_ATTR ringMsgRxCbFn(p2pInfo* p2p, char* msg, uint8_t* payload __attribute__((unused)), uint8_t len)
{
    if(0 == strcmp(msg, "tst"))
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

    os_snprintf(lastMsg, sizeof(lastMsg), "Received %d bytes from %s\n", len, getRingConnection(p2p)->lbl);
    os_printf("%s", lastMsg);
    ringUpdateDisplay();
    return;
}

/**
 * @brief TODO
 *
 * @param p2p
 * @param status
 */
void ICACHE_FLASH_ATTR ringMsgTxCbFn(p2pInfo* p2p, messageStatus_t status)
{
    char* conStr = getRingConnection(p2p)->lbl;

    switch(status)
    {
        case MSG_ACKED:
        {
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: MSG_ACKED\n", conStr);
            break;
        }
        case MSG_FAILED:
        {
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: MSG_FAILED\n", conStr);
            getRingConnection(p2p)->side = 0xFF;
            break;
        }
        default:
        {
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: Unknown status %d\n", conStr, status);
            break;
        }
    }
    os_printf("%s", lastMsg);
    ringUpdateDisplay();
    return;
}

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR ringUpdateDisplay(void)
{
    clearDisplay();

    plotText(6, 0, lastMsg, IBM_VGA_8, WHITE);

    if(NULL != getSideConnection(RIGHT))
    {
        plotRect(OLED_WIDTH - 5, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
    }

    if(NULL != getSideConnection(LEFT))
    {
        plotRect(0, 0, 4, OLED_HEIGHT - 1, WHITE);
    }

    if(radiusRight > 0)
    {
        plotCircle(OLED_WIDTH - 1 - 20, OLED_HEIGHT / 2, radiusRight, WHITE);
    }
    if(radiusLeft > 0)
    {
        plotCircle(20, OLED_HEIGHT / 2, radiusLeft, WHITE);
    }
}

void ICACHE_FLASH_ATTR ringAnimationTimer(void* arg __attribute__((unused)))
{
    bool shouldUpdate = false;
    if(radiusLeft > 0)
    {
        radiusLeft++;
        if(radiusLeft == 20)
        {
            radiusLeft = 0;
        }

        shouldUpdate = true;
    }

    if(radiusRight > 0)
    {
        radiusRight++;
        if(radiusRight == 20)
        {
            radiusRight = 0;
        }
        shouldUpdate = true;
    }

    if(shouldUpdate)
    {
        ringUpdateDisplay();
    }
}
