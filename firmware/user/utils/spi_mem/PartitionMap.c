#include <osapi.h>
#include <user_interface.h>

#include "PartitionMap.h"

/*==============================================================================
 * Partition Map Data
 *============================================================================*/

// #define SYSTEM_PARTITION_OTA_SIZE_OPT2                 0x6A000
// #define SYSTEM_PARTITION_OTA_2_ADDR_OPT2               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT2              0xfb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT2            0xfc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT2    0xfd000
// #define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT2 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT2                        2

// #define SYSTEM_PARTITION_OTA_SIZE_OPT3                 0x6A000
// #define SYSTEM_PARTITION_OTA_2_ADDR_OPT3               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT3              0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3            0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT3    0x1fd000
// #define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT3 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT3                        3

// #define SYSTEM_PARTITION_OTA_SIZE_OPT4                 0x6A000
// #define SYSTEM_PARTITION_OTA_2_ADDR_OPT4               0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR_OPT4              0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR_OPT4            0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT4    0x3fd000
// #define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR_OPT4 0x7c000
#define SPI_FLASH_SIZE_MAP_OPT4                        4

// #define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM SYSTEM_PARTITION_CUSTOMER_BEGIN
#define EAGLE_FLASH_BIN_ADDR                 SYSTEM_PARTITION_CUSTOMER_BEGIN + 1
#define EAGLE_IROM0TEXT_BIN_ADDR             SYSTEM_PARTITION_CUSTOMER_BEGIN + 2

static const partition_item_t partition_table_opt2[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0xBC000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT2,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT2,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT2, 0x3000},
};

static const partition_item_t partition_table_opt3[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0xC0000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT3,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT3, 0x3000},
};

static const partition_item_t partition_table_opt4[] =
{
    { EAGLE_FLASH_BIN_ADDR,              0x00000,                                     0x10000},
    { EAGLE_IROM0TEXT_BIN_ADDR,          0x10000,                                     0xC0000},
    { SYSTEM_PARTITION_RF_CAL,           SYSTEM_PARTITION_RF_CAL_ADDR_OPT4,           0x1000},
    { SYSTEM_PARTITION_PHY_DATA,         SYSTEM_PARTITION_PHY_DATA_ADDR_OPT4,         0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR_OPT4, 0x3000},
};

/**
 * Required function as of ESP8266_NONOS_SDK_v3.0.0. Must call
 * system_partition_table_regist(). This tries to register a few different
 * partition maps. The ESP should be happy with one of them.
 */
void ICACHE_FLASH_ATTR LoadDefaultPartitionMap(void)
{
    if(system_partition_table_regist(
                partition_table_opt2,
                sizeof(partition_table_opt2) / sizeof(partition_table_opt2[0]),
                SPI_FLASH_SIZE_MAP_OPT2))
    {
        os_printf("system_partition_table_regist 2 success!!\r\n");
    }
    else if(system_partition_table_regist(
                partition_table_opt3,
                sizeof(partition_table_opt3) / sizeof(partition_table_opt3[0]),
                SPI_FLASH_SIZE_MAP_OPT3))
    {
        os_printf("system_partition_table_regist 3 success!!\r\n");
    }
    else if(system_partition_table_regist(
                partition_table_opt4,
                sizeof(partition_table_opt4) / sizeof(partition_table_opt4[0]),
                SPI_FLASH_SIZE_MAP_OPT4))
    {
        os_printf("system_partition_table_regist 4 success!!\r\n");
    }
    else
    {
        os_printf("system_partition_table_regist all fail\r\n");
        while(1);
    }
}
