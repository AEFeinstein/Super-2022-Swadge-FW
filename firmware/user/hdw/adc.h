#ifndef __ADC_H__
#define __ADC_H__

#include "user_config.h"

#if defined(FEATURE_MIC)
    void hs_adc_start(void);
    uint16 hs_adc_read(void);
#endif

#endif

