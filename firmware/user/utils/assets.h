#ifndef _ASSETS_H_
#define _ASSETS_H_

uint32_t* getAsset(char* name, uint32_t* retLen);
void drawBitmapFromAsset(char* name, int16_t x, int16_t y, bool flipLR);

#endif