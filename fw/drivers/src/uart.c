#include "uart.h"

typedef struct {
    volatile uint32_t START_TRANSMIT;         // 0x000
    uint32_t reserved0;                       // 0x004
    volatile uint32_t SIZE_TO_TRANSMIT;       // 0x008
    volatile const uint32_t TRANSMISION_DONE; // 0x00C
    volatile uint32_t BUFF_OUT[256];          // 0x010
    uint32_t reserved1[252];                  // 0x410 a 0x7FF
    volatile const uint32_t DATA_AVAILABLE;   // 0x800
    volatile uint32_t DATA_FLUSH;             // 0x804
    volatile const uint32_t INDEX_IN;         // 0x808
    volatile const uint32_t BUFF_IN[256];     // 0x80C
} uart0_regs_t;

#define UART0_REGS ((uart0_regs_t *)0x00042000u)


uint8_t uart_init(){
    while (UART0_REGS->TRANSMISION_DONE == 0) {
    }
    UART0_REGS->START_TRANSMIT = 0;
    UART0_REGS->SIZE_TO_TRANSMIT = 0;
    return UART0_OK; // Success
}
uint8_t uart_write_byte(char c){

    while (UART0_REGS->TRANSMISION_DONE == 0) {
    }

    UART0_REGS->BUFF_OUT[0] = (uint8_t)c;
    UART0_REGS->SIZE_TO_TRANSMIT = 1;
    UART0_REGS->START_TRANSMIT = 1;
    UART0_REGS->START_TRANSMIT = 0;

    while (UART0_REGS->TRANSMISION_DONE == 0) {
    }

    return UART0_OK;
    }

uint8_t uart_write_str(const char *s) {
    while (*s != '\0') {
        uint8_t estado = uart_write_byte(*s);
        if (estado != UART0_OK) {
            return estado;
        }
        s++;
    }
    return UART0_OK;
}
