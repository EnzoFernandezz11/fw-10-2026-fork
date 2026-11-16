
#include "hal_timer.h"

static uint32_t contador_ms;

/* Reinicia el contador de milisegundos. */
void timer_init(void)
{
    contador_ms = 0;
}

/* Suma un milisegundo al contador. */
void timer_tick(void)
{
    contador_ms++;
}

/* Devuelve los milisegundos acumulados. */
uint32_t timer_get_ms(void)
{
    return contador_ms;
}
