#include "adc.h"
#include "includes.h"

#ifndef ADC_TEST_DIVIDER
#define ADC_TEST_DIVIDER 32u
#endif

static uint32_t samples[REG_ARRAY_ADC_BUFF_SIZE + 2];

void main(void)
{
    REG_FW_STATUS = 0;
    if (adc_read(0, 8) != 0 || adc_read(samples, 0) != 0 ||
        REG_ADC_START != 0 || REG_ADC_CLK_DIVIDER != 0)
        goto fail;
    if (adc_init(16) != ADC_OK ||
        set_adc_clk_divider(0) != ADC_ERR || REG_ADC_CLK_DIVIDER != 16 ||
        set_adc_clk_divider(ADC_TEST_DIVIDER) != ADC_OK ||
        REG_ADC_CLK_DIVIDER != ADC_TEST_DIVIDER ||
        adc_start(ADC_MODE_SINGLE) != ADC_OK)
        goto fail;

    uint32_t timeout = 100000;
    while (!adc_half_done() && timeout > 0)
        timeout--;
    if (timeout == 0 || adc_is_done())
        goto fail;
    timeout = 100000;
    while (!adc_is_done() && timeout > 0)
        timeout--;
    if (timeout == 0)
        goto fail;

    samples[0] = 0x12345678u;
    samples[1] = 0xabcdef01u;
    samples[REG_ARRAY_ADC_BUFF_SIZE + 1] = 0x87654321u;
    if (adc_read(0, 8) != 0 || adc_read(&samples[1], 0) != 0 ||
        samples[1] != 0xabcdef01u ||
        adc_read(&samples[1], 1) != 1 || samples[1] != REG_ARRAY_ADC_BUFF[0] ||
        adc_read(&samples[1], REG_ARRAY_ADC_BUFF_SIZE) != REG_ARRAY_ADC_BUFF_SIZE ||
        adc_read(&samples[1], 260) != REG_ARRAY_ADC_BUFF_SIZE ||
        samples[0] != 0x12345678u || samples[REG_ARRAY_ADC_BUFF_SIZE + 1] != 0x87654321u)
        goto fail;

    for (uint32_t i = 0; i < REG_ARRAY_ADC_BUFF_SIZE; i++)
        if (samples[i + 1] != REG_ARRAY_ADC_BUFF[i])
            goto fail;

    print_str("-- ADC data read --");
    for (uint32_t i = 0; i < REG_ARRAY_ADC_BUFF_SIZE; i++)
        print_dec(samples[i + 1]);

    REG_FW_STATUS = 0xff;
    return;

fail:
    REG_FW_STATUS = 0xbad;
}
