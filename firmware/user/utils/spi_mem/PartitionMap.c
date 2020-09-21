#include <osapi.h>
#include <user_interface.h>

#include "PartitionMap.h"
#include "printControl.h"

/*==============================================================================
 * Partition Map Data
 *============================================================================*/

// #define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM SYSTEM_PARTITION_CUSTOMER_BEGIN
#define EAGLE_FLASH_BIN_ADDR     SYSTEM_PARTITION_CUSTOMER_BEGIN + 1
#define EAGLE_IROM0TEXT_BIN_ADDR SYSTEM_PARTITION_CUSTOMER_BEGIN + 2
#define PRT_USER_SETTINGS_ADDR   SYSTEM_PARTITION_CUSTOMER_BEGIN + 3
#define PRT_ASSETS_ADDR          SYSTEM_PARTITION_CUSTOMER_BEGIN + 4

// The values in this table are defined in the makefile in order to coordinate flashing
static const partition_item_t partition_table[] =
{
    { EAGLE_FLASH_BIN_ADDR,              FW_FILE1_ADDR,      FW_FILE1_SIZE},
    { EAGLE_IROM0TEXT_BIN_ADDR,          FW_FILE2_ADDR,      FW_FILE2_SIZE},
    { PRT_USER_SETTINGS_ADDR,            USER_SETTINGS_ADDR, USER_SETTINGS_SIZE},
    { PRT_ASSETS_ADDR,                   ASSETS_ADDR,        ASSETS_SIZE},
    { SYSTEM_PARTITION_RF_CAL,           RF_CAL_ADDR,        RF_CAL_SIZE},
    { SYSTEM_PARTITION_PHY_DATA,         PHY_DATA_ADDR,      PHY_DATA_SIZE},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, SYS_PARAM_ADDR,     SYS_PARAM_SIZE},
};

/**
 * Required function as of ESP8266_NONOS_SDK_v3.0.0. Must call
 * system_partition_table_regist(). This tries to register a few different
 * partition maps. The ESP should be happy with one of them.
 *
 * WARNING
 * -------
 * This project uses the ESP-WROOM-02D, which has 16mbit SPI flash.
 * This corresponds to partition table option 3. How
 *
 * Table 4-2 in the following PDF notes that for a 2MB flash map,
 * esp_init_data_default_v08.bin gets flashed at 0x1FC000
 * https://www.espressif.com/sites/default/files/documentation/2a-esp8266-sdk_getting_started_guide_en.pdf
 *
 * This data corresponds to SYSTEM_PARTITION_PHY_DATA_ADDR_OPT3, which is also
 * 0x1FC000. The data at this address is detailed in the following PDF
 * https://www.espressif.com/sites/default/files/documentation/esp8266_phy_init_bin_parameter_configuration_guide_en.pdf
 *
 * All of these addresses must match to work.
 */
void ICACHE_FLASH_ATTR LoadDefaultPartitionMap(void)
{
    // 16 mbit is most common, so try that first
    if(system_partition_table_regist(
           partition_table,
           sizeof(partition_table) / sizeof(partition_table[0]),
           FLASH_SIZE_16M_MAP_512_512))
    {
        INIT_PRINTF("system_partition_table_regist success!!\r\n");
    }
    else
    {
        INIT_PRINTF("system_partition_table_regist all fail\r\n");
        while(1);
    }
}
