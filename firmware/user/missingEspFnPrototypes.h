/*
 * missingEspFnPrototypes.h
 *
 *  Created on: Oct 26, 2018
 *      Author: adam
 */

#ifndef USER_MISSINGESPFNPROTOTYPES_H_
#define USER_MISSINGESPFNPROTOTYPES_H_

void ets_isr_mask(unsigned intr);
void ets_isr_unmask(unsigned intr);
void rom_i2c_writeReg_Mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);
void read_sar_dout(uint16_t* );
uint32_t xthal_get_ccount(void);

#endif /* USER_MISSINGESPFNPROTOTYPES_H_ */
