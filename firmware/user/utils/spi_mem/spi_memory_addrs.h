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
#define COMMON_SERVICES_SETTINGS_ADDR 0xBA000
#define COMMON_SERVICES_SETTINGS_SIZE  0x3000
/**
 * Settings used in custom_commands.c. Comes 3 sectors after COMMON_SERVICES_SETTINGS_ADDR
 * Currently 0x7F000
 */
#define USER_SETTINGS_ADDR            (COMMON_SERVICES_SETTINGS_ADDR + COMMON_SERVICES_SETTINGS_SIZE)
#define USER_SETTINGS_SIZE            0x3000

#endif /* SPI_MEMORY_ADDRS_H_ */
