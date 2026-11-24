#ifndef ADC_H
#define ADC_H

#include "config.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    union {
        uint32_t value;
        struct {
            uint32_t start : 1;
            uint32_t circular : 1;
            uint32_t : 30;
        };
    } start;
    const struct {
        uint32_t half_done : 1;
        uint32_t acq_done : 1;
        uint32_t : 30;
    } status;
    uint32_t clk_divider;
    uint32_t reserved[50];
    const uint32_t buffer[REG_ARRAY_ADC_BUFF_SIZE];
} adc_regs_t;

#define REG_ADC ((volatile adc_regs_t *)&REG_ADC_START)

/* Comprueban que los registros esten en sus offsets correctos dentro del struct. */
_Static_assert(offsetof(adc_regs_t, status) == 0x04, "ADC status offset");
_Static_assert(offsetof(adc_regs_t, clk_divider) == 0x08, "ADC divider offset");
_Static_assert(offsetof(adc_regs_t, buffer) == 0xd4, "ADC buffer offset");

#define ADC_OK 0
#define ADC_ERR 1

#define ADC_MODE_SINGLE    0u
#define ADC_MODE_CIRCULAR  1u
#define ADC_DEFAULT_DIVIDER 32u

uint8_t adc_init(uint32_t clk_div);
/* Configurar antes de iniciar una captura; cero no es valido. */
uint8_t set_adc_clk_divider(uint32_t clk_div);
uint8_t adc_start(uint8_t adc_mode);
uint8_t adc_half_done(void);
/* Consulta el estado sin esperar. */
uint8_t adc_is_done(void);
/* Espera a que termine la captura iniciada y devuelve 1. */
uint8_t adc_done(void);
/* Direccion del buffer de muestras para transferencias DMA. */
const void *adc_buffer(void);
/* Copia hasta 256 muestras disponibles y devuelve la cantidad copiada.
 * Si data es NULL o size es cero, devuelve 0. Si aun no se inicio una
 * captura, inicia una unica y espera su fin (divisor por defecto: 32).
 * Si ya se inicio, el llamador espera las muestras antes de leerlas. */
uint32_t adc_read(uint32_t *data, uint32_t size);

#endif
