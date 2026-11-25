/* Driver de la UART1 serie.
 *
 * Punto 3.b del TP5: completar los cuerpos marcados con TODO usando los registros de
 *         datasheets/UART1_DATASHEET.md (REG_UART1_*, definidos en config.h).
 * TP6: completar uart1_set_rts().
 *
 * Hoy los cuerpos no hacen nada (solo para que el proyecto compile): make
 * test_uart_serial tiene que fallar hasta que los implementes.
 */
#include "uart1.h"

static uint32_t uart1_rts_on = 0;

void uart1_init(uint32_t baud_div)
{
    if (baud_div < 2) {
        return;
    }
    uart1_rts_on = 0;
    REG_UART1_BAUD_DIV = baud_div;
    REG_UART1_CTRL = UART1_CTRL_CLR_ERR;
}

uint32_t uart1_rx_level(void)
{
    return UART1_ST_RX_LEVEL(REG_UART1_STATUS);
}

int uart1_getc(uint8_t *b)
{
    uint32_t dato = REG_UART1_RX_DATA;

    if (dato & UART1_RX_VALID) {
        *b = (uint8_t)dato;
        return 1;
    }

    return 0;
}

void uart1_putc(uint8_t b)
{
    while (REG_UART1_STATUS & UART1_ST_TX_FULL) { }
    REG_UART1_TX_DATA = b;
}

void uart1_write(const uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uart1_putc(buf[i]);
    }
}

int uart1_tx_idle(void)
{
    return (REG_UART1_STATUS & UART1_ST_TX_IDLE) ? 1 : 0;
}

uint32_t uart1_errors(void)
{
    uint32_t status = REG_UART1_STATUS;
    uint32_t errores = 0;

    if (status & UART1_ST_FRAME_ERR) {
        errores |= UART1_ERR_FRAME;
    }

    if (status & UART1_ST_OVERRUN) {
        errores |= UART1_ERR_OVERRUN;
    }

    return errores;
}

void uart1_clear_errors(void)
{
    REG_UART1_CTRL = uart1_rts_on | UART1_CTRL_CLR_ERR;
}

/* ---- TP6 ---- */

void uart1_set_rts(int ready)
{
    if(ready){
        uart1_rts_on = UART1_CTRL_RTS;
        REG_UART1_CTRL = UART1_CTRL_RTS;
    }
    else{
        uart1_rts_on = 0;
        REG_UART1_CTRL = 0;
    }
}
