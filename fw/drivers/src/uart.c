#include "uart.h"
#include "config.h"

uint8_t uart_init(void) {
    while (REG_UART0_TRANSMISION_DONE == 0) { }
    REG_UART0_START_TRANSMIT = 0;
    REG_UART0_SIZE_TO_TRANSMIT = 0;
    return UART0_OK;
}

uint8_t uart_write_byte(char c) {
    while (REG_UART0_TRANSMISION_DONE == 0) { }

    REG_ARRAY_UART0_BUFF_OUT[0] = (uint8_t)c;
    REG_UART0_SIZE_TO_TRANSMIT = 1;
    REG_UART0_START_TRANSMIT = 1;
    REG_UART0_START_TRANSMIT = 0;

    while (REG_UART0_TRANSMISION_DONE == 0) { }

    return UART0_OK;
}

uint8_t uart_write_str(const char *s) {
    if (s == 0) {
        return UART0_ERR;
    }

    while (*s != '\0') {
        uint32_t size = 0;
        while (REG_UART0_TRANSMISION_DONE == 0) { }
        while (size < REG_ARRAY_UART0_BUFF_OUT_SIZE && *s != '\0') {
            REG_ARRAY_UART0_BUFF_OUT[size++] = (uint8_t)*s++;
        }
        REG_UART0_SIZE_TO_TRANSMIT = size;
        REG_UART0_START_TRANSMIT = 1;
        REG_UART0_START_TRANSMIT = 0;
        while (REG_UART0_TRANSMISION_DONE == 0) { }
    }
    return UART0_OK;
}
