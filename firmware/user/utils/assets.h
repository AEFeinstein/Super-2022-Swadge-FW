#ifndef _ASSETS_H_
#define _ASSETS_H_

#include "synced_timer.h"
#include "user_config.h"
#include "oled.h"

#if defined(FEATURE_OLED)

uint32_t* getAsset(const char* name, uint32_t* retLen);

#if defined(EMU)
    void ICACHE_FLASH_ATTR freeAssets(void);
#endif

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint32_t dataLen;
    uint32_t* data;
} pngHandle;

bool ICACHE_FLASH_ATTR allocPngAsset(const char* name, pngHandle* handle);
void ICACHE_FLASH_ATTR freePngAsset(pngHandle* handle);
void ICACHE_FLASH_ATTR drawPng(pngHandle* handle, int16_t xp,
                               int16_t yp, bool flipLR, bool flipUD, int16_t rotateDeg);
void ICACHE_FLASH_ATTR drawPngToBuffer(pngHandle* handle, color* buf);

typedef struct
{
    uint16_t count;
    pngHandle* handles;
    uint16_t cFrame;
} pngSequenceHandle;

bool ICACHE_FLASH_ATTR allocPngSequence(pngSequenceHandle* handle, uint16_t count, ...);
void ICACHE_FLASH_ATTR freePngSequence(pngSequenceHandle* handle);
void ICACHE_FLASH_ATTR drawPngSequence(pngSequenceHandle* handle, int16_t xp,
                                       int16_t yp, bool flipLR, bool flipUD, int16_t rotateDeg, int16_t frame);

typedef struct
{
    uint32_t* assetPtr;
    uint32_t idx;

    uint8_t* compressed;
    uint8_t* decompressed;
    uint8_t* frame;
    uint32_t allocedSize;

    uint16_t width;
    uint16_t height;

    uint16_t nFrames;
    uint16_t cFrame;
    uint16_t duration;

    bool firstFrameLoaded;
} gifHandle;

void loadGifFromAsset(const char* name, gifHandle* handle);
void drawGifFromAsset(gifHandle* handle, int16_t xp, int16_t yp,
                      bool flipLR, bool flipUD, int16_t rotateDeg, bool drawNext);
void freeGifAsset(gifHandle* handle);

#endif
#endif