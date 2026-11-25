#ifndef UART_H
#define UART_H

#include <stdint.h>

#define UART0_OK  0u
#define UART0_ERR 1u

uint8_t uart_init(void);
uint8_t uart_write_byte(char c);
uint8_t uart_write_str(const char *s);

#endif
