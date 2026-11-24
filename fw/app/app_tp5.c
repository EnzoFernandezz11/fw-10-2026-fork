#include "adc.h"
#include "dma.h"
#include "led.h"
#include "uart.h"

/* Demo continua de ADC, DMA, LED y UART:
 * 1. Configura los drivers una vez e inicia una captura unica de 256 muestras.
 * 2. Cuando el ADC indica que la primera mitad esta lista, copia esas 128
 *    muestras al buffer circular mediante dos transferencias DMA de 64.
 *    Mientras se procesa esta mitad, el ADC puede seguir llenando la segunda.
 * 3. Consume las muestras en orden y comprueba la copia contra el buffer ADC;
 *    luego enciende el LED y envia un mensaje de estado por UART.
 * 4. Espera el fin de la captura y repite la copia y validacion con la segunda
 *    mitad. Despues inicia una nueva captura y repite el ciclo.
 *
 * El buffer circular tiene 128 posiciones y usa indices separados de lectura
 * y escritura, que vuelven a cero al llegar al final. El contador disponibles
 * evita sobrescribir muestras pendientes: si no hay espacio, se informa error
 * y se detiene la demo. Cada mitad se copia y consume de forma secuencial;
 * entre capturas puede haber intervalos sin adquirir muestras. */
#define TOTAL_SAMPLES 128u
#define DMA_BLOCK_SAMPLES 64u
#define ADC_DIVISOR 256u

static uint32_t muestras[TOTAL_SAMPLES];
static uint32_t referencia[TOTAL_SAMPLES * 2u];
static uint32_t posicion_escritura;
static uint32_t posicion_lectura;
static uint32_t disponibles;

static int procesar_mitad(uint32_t offset)
{
    const uint32_t *origen = adc_buffer();

    if (adc_read(referencia, offset + TOTAL_SAMPLES) != offset + TOTAL_SAMPLES)
        return 1;

    for (uint32_t i = 0; i < TOTAL_SAMPLES; i += DMA_BLOCK_SAMPLES) {
        /* No sobrescribir muestras todavia no consumidas. */
        if (TOTAL_SAMPLES - disponibles < DMA_BLOCK_SAMPLES)
            return 1;
        if (dma_copy(origen + offset + i, muestras + posicion_escritura,
                     DMA_BLOCK_SAMPLES) != DMA_OK)
            return 1;
        posicion_escritura = (posicion_escritura + DMA_BLOCK_SAMPLES) % TOTAL_SAMPLES;
        disponibles += DMA_BLOCK_SAMPLES;
    }

    for (uint32_t i = 0; i < TOTAL_SAMPLES; i++) {
        if (muestras[posicion_lectura] != referencia[offset + i])
            return 1;
        posicion_lectura = (posicion_lectura + 1) % TOTAL_SAMPLES;
        disponibles--;
    }
    return 0;
}

int main(void)
{
    led_init();
    if (uart_init() != UART0_OK || dma_init() != DMA_OK ||
        adc_init(ADC_DIVISOR) != ADC_OK)
        return 1;

    uart_write_str("Demo TP5 iniciada\n");
    while(1) {
        led_off(0);
        if (adc_start(ADC_MODE_SINGLE) != ADC_OK)
            break;
        while (!adc_half_done()) { }

        if (procesar_mitad(0) != 0)
            break;
        led_on(0);
        uart_write_str("Primeras 128 muestras listas\n");

        while (!adc_is_done()) { }
        led_off(0);
        if (procesar_mitad(TOTAL_SAMPLES) != 0)
            break;
        led_on(0);
        uart_write_str("Siguientes 128 muestras listas\n");
    }
    uart_write_str("Error en demo TP5\n");
    return 1;
}
