#include "adc.h"

uint8_t adc_init(uint32_t clk_div) {
    if (clk_div == 0) {
        return ADC_ERR;
    }
    REG_ADC->start.value = 0;
    return set_adc_clk_divider(clk_div);
}

uint8_t set_adc_clk_divider(uint32_t clk_div) {
    if (clk_div == 0) {
        return ADC_ERR;
    }
    REG_ADC->clk_divider = clk_div;
    return ADC_OK;
}

/* adc_mode se escribe tal cual en REG_ADC_START (bit 0 = inicio, bit 1 = circular).
 * Primero 0 para que haya flanco 0->1 en el bit de inicio. */
uint8_t adc_start(uint8_t adc_mode)
{
    if (adc_mode & ~(ADC_START_BIT | ADC_CIRCULAR_BIT)) {
        return ADC_ERR;
    }
    REG_ADC->start.value = 0;
    REG_ADC->start.value = adc_mode;
    return ADC_OK;
}

uint8_t adc_half_done(void) {
    while ((REG_ADC_STATUS & ADC_STATUS_HALF_DONE) == 0) { }
    return 1;
}

uint8_t adc_is_done(void) {
    return (REG_ADC_STATUS & ADC_STATUS_ACQ_DONE) ? 1 : 0;
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

    /* Adquisicion completa con el divisor ya configurado por adc_init(). */
    adc_start(ADC_MODE_SINGLE);
    adc_done();

    for (uint32_t i = 0; i < size; i++) {
        data[i] = REG_ADC->buffer[i];
    }
    return size;
}
