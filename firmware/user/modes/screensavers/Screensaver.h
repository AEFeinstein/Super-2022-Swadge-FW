#ifndef _SCREENSAVER_H_
#define _SCREENSAVER_H_

typedef struct
{
    void (*initScreensaver)(void);
    void (*updateScreensaver)(void);
    void (*destroyScreensaver)(void);
} screensaver;

#endif // _SCREENSAVER_H_