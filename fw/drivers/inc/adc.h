#include "config.h"
#include <stdint.h>

#define ADC_OK 0
#define ADC_ERR 1

#define ADC_MODE_SINGLE    0u
#define ADC_MODE_CIRCULAR  1u

uint8_t adc_init(uint32_t clk_div);
uint8_t adc_start(uint8_t adc_mode);
uint8_t adc_half_done(void);
uint8_t adc_done(void);
/* Direccion del buffer de muestras para transferencias DMA. */
const void *adc_buffer(void);
/* Leer solo cuando las muestras solicitadas esten listas. */
uint8_t adc_read(uint32_t *data, uint32_t size);
