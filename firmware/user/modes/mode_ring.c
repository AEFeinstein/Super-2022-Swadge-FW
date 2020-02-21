#include "mode_ring.h"
#include "p2pConnection.h"
#include "buttons.h"
#include "bresenham.h"
#include "font.h"

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

p2pInfo cn0;
p2pInfo cn1;

button_mask cn0connectedSide;
button_mask cn1connectedSide;

button_mask connectionSide;

char lastMsg[256];

os_timer_t animationTimer;
uint8_t radiusLeft = 0;
uint8_t radiusRight = 0;

/**
 * @brief TODO
 *
 */
void ICACHE_FLASH_ATTR ringEnterMode(void)
{
    cn0connectedSide = 0xFF;
    cn1connectedSide = 0xFF;

    ets_memset(&cn0, 0, sizeof(p2pInfo));
    ets_memset(&cn1, 0, sizeof(p2pInfo));

    p2pInitialize(&cn0, "cn0", ringConCbFn, ringMsgRxCbFn, 0);
    p2pInitialize(&cn1, "cn1", ringConCbFn, ringMsgRxCbFn, 0);

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
    p2pDeinit(&cn0);
    p2pDeinit(&cn1);
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

                // If no one's connected on the left
                if(cn0connectedSide != side && cn1connectedSide != side)
                {
                    // Start connections for unconnected p2ps
                    connectionSide = side;
                    if(0xFF == cn0connectedSide)
                    {
                        p2pStartConnection(&cn0);
                    }
                    if(0xFF == cn1connectedSide)
                    {
                        p2pStartConnection(&cn1);
                    }
                }
                else
                {
                    // Otherwise send a message
                    if(side == cn0connectedSide)
                    {
                        p2pSendMsg(&cn0, "tst", "CN0 Test", sizeof("CN0 Test"), ringMsgTxCbFn);

                    }
                    else
                    {
                        p2pSendMsg(&cn1, "tst", "CN1 Test", sizeof("CN1 Test"), ringMsgTxCbFn);
                    }
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
    p2pRecvCb(&cn0, mac_addr, data, len, rssi);
    p2pRecvCb(&cn1, mac_addr, data, len, rssi);
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
    p2pSendCb(&cn0, mac_addr, status);
    p2pSendCb(&cn1, mac_addr, status);
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
    char* conStr = (p2p == &cn0) ? "cn0" : "cn1";

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
            if(p2p == &cn0)
            {
                p2pStopConnection(&cn1);
            }
            else
            {
                p2pStopConnection(&cn0);
            }

            os_snprintf(lastMsg, sizeof(lastMsg), "%s: RX_GAME_START_ACK\n", conStr);
            break;
        }
        case RX_GAME_START_MSG:
        {
            if(p2p == &cn0)
            {
                p2pStopConnection(&cn1);
            }
            else
            {
                p2pStopConnection(&cn0);
            }

            os_snprintf(lastMsg, sizeof(lastMsg), "%s: RX_GAME_START_MSG\n", conStr);
            break;
        }
        case CON_ESTABLISHED:
        {
            if(p2p == &cn0)
            {
                cn0connectedSide = connectionSide;
            }
            else
            {
                cn1connectedSide = connectionSide;
            }
            os_snprintf(lastMsg, sizeof(lastMsg), "%s: CON_ESTABLISHED\n", conStr);
            break;
        }
        case CON_LOST:
        {
            if(p2p == &cn0)
            {
                cn0connectedSide = 0xFF;
            }
            else
            {
                cn1connectedSide = 0xFF;
            }
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
void ICACHE_FLASH_ATTR ringMsgRxCbFn(p2pInfo* p2p, char* msg, uint8_t* payload, uint8_t len)
{
    if(((p2p == &cn0) && (cn0connectedSide == RIGHT)) ||
            ((p2p == &cn1) && (cn1connectedSide == RIGHT)))
    {
        radiusRight = 1;
    }
    else if(((p2p == &cn0) && (cn0connectedSide == LEFT)) ||
            ((p2p == &cn1) && (cn1connectedSide == LEFT)))
    {
        radiusLeft = 1;
    }

    os_snprintf(lastMsg, sizeof(lastMsg), "Received %d bytes from %s\n", len, (p2p == &cn0) ? "cn0" : "cn1");
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
    char* conStr = (p2p == &cn0) ? "cn0" : "cn1";

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

            if(p2p == &cn0)
            {
                cn0connectedSide = 0xFF;
            }
            else
            {
                cn1connectedSide = 0xFF;
            }

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

    if(cn0connectedSide == RIGHT || cn1connectedSide == RIGHT)
    {
        plotRect(OLED_WIDTH - 5, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1, WHITE);
    }

    if(cn0connectedSide == LEFT || cn1connectedSide == LEFT)
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
