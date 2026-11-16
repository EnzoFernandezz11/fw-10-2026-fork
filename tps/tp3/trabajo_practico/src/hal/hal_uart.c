#include "hal_uart.h"

/* Configura la velocidad y habilita la UART simulada. */
void uart_init(uart_regs_t *uart, uint32_t baudrate)
{
    uart->BRR = UART_PERIPH_CLK_HZ / baudrate;
    uart->CR1 = bit_set(uart->CR1, 13);
    uart->CR1 = bit_set(uart->CR1, 3);
    uart->CR1 = bit_set(uart->CR1, 2);
    uart->SR = 0;
    uart->DR = 0;
}

/* Escribe un byte y marca la transmisión como completa. */
void uart_send_byte(uart_regs_t *uart, uint8_t byte)
{
    uart->DR = byte;
    uart->SR = bit_set(uart->SR, 6);
}

/* Envía cada byte del buffer mediante uart_send_byte. */
void uart_send(uart_regs_t *uart, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        uart_send_byte(uart, data[i]);
}
