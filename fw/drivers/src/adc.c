#include "adc.h"

uint8_t adc_init(uint32_t clk_div) {
    if (clk_div == 0) {
        return ADC_ERR;
    }
    REG_ADC_START = 0;
    REG_ADC_CLK_DIVIDER = clk_div;
    return ADC_OK;
}

uint8_t adc_start(uint8_t adc_mode)
{
    if (adc_mode == ADC_MODE_SINGLE) {
        REG_ADC_START = 0;
        REG_ADC_START = 1;
    } else if (adc_mode == ADC_MODE_CIRCULAR) {
        REG_ADC_START = 2;
    } else {
        return ADC_ERR;
    }

    return ADC_OK;
}

uint8_t adc_half_done(void) {
    return (REG_ADC_STATUS & 0x1);
}

uint8_t adc_done(void) {
    return ((REG_ADC_STATUS & 0x2) ? 1 : 0);
}

const void *adc_buffer(void) {
    return (const void *)REG_ARRAY_ADC_BUFF_BASE;
}

uint8_t adc_read(uint32_t *data, uint32_t size) {
    if ((size > 0 && data == 0) || size > REG_ARRAY_ADC_BUFF_SIZE) {
        return ADC_ERR;
    }
    for (uint32_t i = 0; i < size; i++) {
        data[i] = REG_ARRAY_ADC_BUFF[i];
    }
    return ADC_OK;
}
