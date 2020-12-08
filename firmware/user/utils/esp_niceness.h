#ifndef _ESP_NICENESS_H
#define _ESP_NICENESS_H

// Prototypes missing from the SDK
void rom_i2c_writeReg_Mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);
void read_sar_dout(uint16_t* );
uint32_t xthal_get_ccount(void);
char* ets_strcat(char* dest, const char* src);
uint32_t system_get_rtc_time(void); //XXX TESTME (note, probably inaccurate, will also need system_rtc_clock_cali_proc) "If system_get_rtc_time returns 10 (which means 10 RTC cycles), and system_rtc_clock_cali_proc returns 5.75 (which means 5.75 μs per RTC cycle), the real time is 10 x 5.75 = 57.5 μs.
uint32_t hw_timer_get_count_data(void); //XXX TESTME
uint32_t system_get_time(void); //time in microseconds maybe?

// Redefine common functions as their ESP equivalents
#define bzero ets_bzero
#define delay_us ets_delay_us
#define get_cpu_frequency ets_get_cpu_frequency
#define install_putc1 ets_install_putc1
#define intr_lock ets_intr_lock
#define intr_unlock ets_intr_unlock
#define isr_attach ets_isr_attach
#define isr_mask ets_isr_mask
#define isr_unmask ets_isr_unmask
#define memcmp ets_memcmp
#define memcpy ets_memcpy
#define memmove ets_memmove
#define memset ets_memset
#define post ets_post
#define printf os_printf
#define run ets_run
#define set_idle_cb ets_set_idle_cb
#if !defined(EMU)
    #define snprintf ets_snprintf
    #define sprintf ets_sprintf
    #define putc ets_putc
#endif
#define strcat ets_strcat
#define strchr ets_strchr
#define strcmp ets_strcmp
#define strcpy ets_strcpy
#define strdup ets_strdup
#define strlen ets_strlen
#define strncmp ets_strncmp
#define strncpy ets_strncpy
#define strrchr ets_strrchr
#define strstr ets_strstr
#define task ets_task
#define timer_arm_new ets_timer_arm_new
#define timer_disarm ets_timer_disarm
#define timer_done ets_timer_done
#define timer_handler_isr ets_timer_handler_isr
#define timer_init ets_timer_init
#define timer_setfn ets_timer_setfn
#define update_cpu_frequency ets_update_cpu_frequency
#define vprintf ets_vprintf
#define vsnprintf ets_vsnprintf
#define vsprintf ets_vsprintf
#define write_char ets_write_char

#define OVERCLOCK_SECTION_ENABLE()  REG_SET_BIT(0x3ff00014, BIT(0));
#define OVERCLOCK_SECTION_DISABLE() REG_CLR_BIT(0x3ff00014, BIT(0));

#endif
