#ifndef _PRINT_CONTROL_H_
#define _PRINT_CONTROL_H_

#include "osapi.h"

/* The following chars are not printed by the system during normal boot. They
 * can be used to print searchable single chars when debugging
 * 
 * ! " # $ % & ' * + - . / ; < = > ? @ G P Q
 * V W Y Z [ \ ] ^ _ ` g p q v w y z { | } ~
 */

/*==============================================================================
 * These defines turn debugging on
 *============================================================================*/

// #define ALL_OS_PRINTF

// #define EXTRA_ESPNOW_DEBUG
// #define P2P_DEBUG_PRINT
// #define SWADGEPASS_DBG

// #define INIT_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define ENOW_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define P2P_PRINTF(fmt, ...)  os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define BZR_PRINTF(fmt, ...)  os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define ACC_PRINTF(fmt, ...)  os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define AST_PRINTF(fmt, ...)  os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define RING_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define PET_PRINTF(fmt, ...)  os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
// #define TIME_PRINTF(fmt, ...) os_printf("%s::%d " fmt, __func__, __LINE__, ##__VA_ARGS__)

/*==============================================================================
 * These defines turn debugging off
 *============================================================================*/

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