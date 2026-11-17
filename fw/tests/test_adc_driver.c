#include "adc.h"
#include "includes.h"

static uint32_t samples[REG_ARRAY_ADC_BUFF_SIZE];

void main(void)
{
    REG_FW_STATUS = 0;
    if (adc_init(4) != ADC_OK || adc_start(ADC_MODE_SINGLE) != ADC_OK)
        goto fail;

    uint32_t timeout = 100000;
    while (!adc_done() && timeout > 0)
        timeout--;
    if (timeout == 0 || adc_read(samples, REG_ARRAY_ADC_BUFF_SIZE) != ADC_OK)
        goto fail;

    print_str("-- ADC data read --");
    for (uint32_t i = 0; i < REG_ARRAY_ADC_BUFF_SIZE; i++)
        print_dec(samples[i]);

    REG_FW_STATUS = 0xff;
    return;

fail:
    REG_FW_STATUS = 0xbad;
}
