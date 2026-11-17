#include <stdint.h>

#define UART0_OK  0
#define UART0_ERR 1


uint8_t uart_init();
uint8_t uart_write_byte(char c);
uint8_t uart_write_str(const char *s);
