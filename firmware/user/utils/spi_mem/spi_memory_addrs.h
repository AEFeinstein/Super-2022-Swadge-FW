/*
 * spi_memory_addrs.h
 *
 *  Created on: Nov 15, 2018
 *      Author: adam
 */

#ifndef SPI_MEMORY_ADDRS_H_
#define SPI_MEMORY_ADDRS_H_

/**
 * See SETTINGS_ADDR in commonservices.c and
 * SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM_ADDR in user_main.c
 * system_param_load() requires three sectors (3 * 0x1000)
 */
#define COMMON_SERVICES_SETTINGS_ADDR 0x7C000
#define COMMON_SERVICES_SETTINGS_SIZE  0x3000

/**
 * Settings used in nvm_interface.c. Comes 3 sectors after COMMON_SERVICES_SETTINGS_ADDR
 * Currently 0x7F000
 */
#define USER_SETTINGS_ADDR            (COMMON_SERVICES_SETTINGS_ADDR + COMMON_SERVICES_SETTINGS_SIZE)
#define USER_SETTINGS_SIZE            0x3000

/**
 * For 2MB SPI flash, irom0 is up to 0xC0000 bytes, see partition_table_opt3[]
 * The linker only allocates 0x5C000 for irom0, see irom0_0_seg in eagle.app.v6.ld
 * The difference is space for an assets file
 * For now, assets come after irom0, before COMMON_SERVICES_SETTINGS_ADDR
 * 
 * TODO: Does this overlap with common common services or user settings?
 */
#define IROM_0_ADDR  0x10000
#define IROM_0_SIZE  0x5C000
#define MAX_ROM_SIZE 0xC0000
#define ASSETS_ADDR  (IROM_0_ADDR + IROM_0_SIZE)
#define ASSETS_SIZE  (MAX_ROM_SIZE - IROM_0_SIZE)

#endif /* SPI_MEMORY_ADDRS_H_ */
