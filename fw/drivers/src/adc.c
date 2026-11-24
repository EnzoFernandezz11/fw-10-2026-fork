#include "adc.h"

static uint8_t adc_started;

uint8_t adc_init(uint32_t clk_div) {
    if (clk_div == 0) {
        return ADC_ERR;
    }
    REG_ADC->start.value = 0;
    adc_started = 0;
    return set_adc_clk_divider(clk_div);
}

uint8_t set_adc_clk_divider(uint32_t clk_div) {
    if (clk_div == 0) {
        return ADC_ERR;
    }
    REG_ADC->clk_divider = clk_div;
    return ADC_OK;
}

uint8_t adc_start(uint8_t adc_mode)
{
    if (adc_mode == ADC_MODE_SINGLE) {
        REG_ADC->start.value = 0;
        REG_ADC->start.value = 1;
    } else if (adc_mode == ADC_MODE_CIRCULAR) {
        REG_ADC->start.value = 2;
    } else {
        return ADC_ERR;
    }

    adc_started = 1;
    return ADC_OK;
}

uint8_t adc_half_done(void) {
    return REG_ADC->status.half_done;
}

uint8_t adc_is_done(void) {
    return REG_ADC->status.acq_done;
}

uint8_t adc_done(void) {
    while (!adc_is_done()) { }
    return 1;
}

const void *adc_buffer(void) {
    return (const void *)REG_ADC->buffer;
}

uint32_t adc_read(uint32_t *data, uint32_t size) {
    if (data == 0 || size == 0) return 0;
    if (size > REG_ARRAY_ADC_BUFF_SIZE) size = REG_ARRAY_ADC_BUFF_SIZE;
    if (!adc_started) {
        if (REG_ADC->clk_divider == 0)
            adc_init(ADC_DEFAULT_DIVIDER);
        adc_start(ADC_MODE_SINGLE);
        adc_done();
    }
    for (uint32_t i = 0; i < size; i++) {
        data[i] = REG_ADC->buffer[i];
    }
    return size;
}
