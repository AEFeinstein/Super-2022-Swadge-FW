# p2pConnection.h

## Overview

The p2p connection protocol is object-oriented-ish. There is a single struct defined in ``p2pConnection.h``, ``p2pInfo``, which contains all the state and timer information for the protocol. The protocol does not allocate any memory for this, it must be provided by the Swadge mode. Every function takes a pointer to this memory as its first argument.

The messages are underscore delimited with the following format:

```mid_typ_sn_XX:XX:XX:XX:XX:XX_payload```
 * ``mid`` - A three char message ID. This ID must be unique for each Swadge Mode and prevents one mode from attempting to process another mode's traffic
 * ``typ`` - A three char message type. This type may be any value for your mode except the following reserved values:
   * ``con`` - A connection broadcast used when pairing
   * ``str`` - A start message used when pairing
   * ``ack`` - An ACK message used when transmitting messages
 * ``sn`` - A two byte ASCII sequence number from 0 to 99. This is not included in ``con`` broadcasts.
 * ``XX:XX:XX:XX:XX:XX`` - A 17 char destination MAC address. After a connection is established, a Swadge will only process messages addressed to its MAC address. This is not included in ``con`` broadcasts.
 * ``payload`` - An optional payload, up to 32 chars. If a Swadge mode only needs to transmit small amounts of data, multiple message types may be sufficient.

## Function Descriptions
```
void ICACHE_FLASH_ATTR p2pInitialize(p2pInfo* p2p, char* msgId,
                                     p2pConCbFn conCbFn,
                                     p2pMsgRxCbFn msgRxCbFn);
```
This function **must** be called to initialize the p2p connection protocol. It should be called once, when the Swadge Mode is initializing.

The Swadge mode must provide a three char ID for all its messages (``char* msgId``). This must be unique per-mode and prevents one mode from processing another mode's messages.

The Swadge mode should provide a function pointer to handle UI based on connection events (``p2pConCbFn conCbFn``). The events that will be reported are:
 * ``CON_STARTED``
 * ``RX_GAME_START_ACK``
 * ``RX_GAME_START_MSG``
 * ``CON_ESTABLISHED``
 * ``CON_LOST``
 
The Swadge mode should provide a function pointer to process received messages (``p2pMsgRxCbFn msgRxCbFn``). Connection messages and ACKs will not be sent to this callback.

----

```
void ICACHE_FLASH_ATTR p2pDeinit(p2pInfo* p2p);
```
This function **must** be called to deinitialize the p2p connection protocol. It stops all timers. It should be called when the Swadge mode is deinitializing, and may be called earlier if you're done with the p2p connection protocol.

----

```
void ICACHE_FLASH_ATTR p2pStartConnection(p2pInfo* p2p);
```
This function **must** be called to to start the connection process. This may not be called before ``p2pInitialize()``.

----

```
void ICACHE_FLASH_ATTR p2pSendCb(p2pInfo* p2p, uint8_t* mac_addr, mt_tx_status status);
```
This function **must** be called by the function registered to the Swadge mode's ``fnEspNowSendCb`` function pointer. This is how the p2p connection protocol knows if a message was transmitted or not. The Swadge Mode should not process any callbacks here directly.

----

```
void ICACHE_FLASH_ATTR p2pRecvMsg(p2pInfo* p2p, uint8_t* mac_addr, uint8_t* data, uint8_t len, uint8_t rssi);
```
This function **must** be called by the function registered to the Swadge mode's ``fnEspNowRecvCb`` function pointer. This is how incoming messages get processed by the p2p connection protocol. The Swadge Mode must not process any incoming messages directly. Incoming messages meant for the Swadge mode will be emitted through the ``p2pMsgRxCbFn msgRxCbFn``, set by ``p2pInitialize()``

----

```
void ICACHE_FLASH_ATTR p2pSendMsg(p2pInfo* p2p, char* msg, char* payload, uint16_t len, p2pMsgTxCbFn msgTxCbFn);
```
This function may be called to send a message to the connected Swadge.

You must provide a three character message type (``char* msg``).

You may provide a payload, up to 32 bytes, and its length (``char* payload``, ``uint16_t len``).

You may provide a function pointer which will be called when the message is either ACKed or dropped. The potential arguments to this callback function are:
 * ``MSG_ACKED``
 * ``MSG_FAILED``
 
----

```
playOrder_t ICACHE_FLASH_ATTR p2pGetPlayOrder(p2pInfo* p2p);
```
This function may be called to figure out the play order of the connected Swadges. This may be used to determine who is the server and who is the client. It will return either:
 * ``NOT_SET``
 * ``GOING_SECOND``
 * ``GOING_FIRST``
 
----

```
void ICACHE_FLASH_ATTR p2pSetPlayOrder(p2pInfo* p2p, playOrder_t order);
```
This function may be called to manually override the play order of the connected Swadges. It is not recommended, but may be useful for implementing a single-player mode.

----
