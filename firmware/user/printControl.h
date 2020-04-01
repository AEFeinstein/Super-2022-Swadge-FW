#ifndef _PRINT_CONTROL_H_
#define _PRINT_CONTROL_H_

#include "osapi.h"

// #define INIT_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define ENOW_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define P2P_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define BZR_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define ACC_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define AST_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define RING_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define PET_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define TIME_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define INIT_PRINTF(fmt, ...)
#define ENOW_PRINTF(fmt, ...)
#define P2P_PRINTF(fmt, ...)
#define BZR_PRINTF(fmt, ...)
#define ACC_PRINTF(fmt, ...)
#define AST_PRINTF(fmt, ...)
#define RING_PRINTF(fmt, ...)
#define PET_PRINTF(fmt, ...)
#define TIME_PRINTF(fmt, ...)

#endif