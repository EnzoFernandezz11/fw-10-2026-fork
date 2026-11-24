#include "config.h"
#include "uart.h"

void main(void) {
    int ok = 1;
    REG_FW_STATUS = 0;

    ok &= (uart_init() == UART0_OK);
    ok &= (uart_write_str("[UART0 TEST BEGIN]\n") == UART0_OK);
    ok &= (uart_write_str("Debug UART0\n") == UART0_OK);

    // Los errores y el string vacio no deben transmitir caracteres.
    ok &= (uart_write_str(0) == UART0_ERR);
    ok &= (uart_write_str("") == UART0_OK);

    // Incluye byte nulo y bytes cuyo bit de signo esta encendido.
    const uint8_t bytes[] = {0x00, 0x7F, 0x80, 0xFF};
    for (unsigned int i = 0; i < 4; i++) {
        ok &= (uart_write_byte((char)bytes[i]) == UART0_OK);
    }

    // Cadenas alrededor del tamano del buffer y mayores que el buffer.
    const unsigned int longitudes[] = {1, 255, 256, 257, 300};
    char texto[301];
    for (unsigned int i = 0; i < 5; i++) {
        unsigned int n = longitudes[i];
        for (unsigned int j = 0; j < n; j++) {
            texto[j] = 'a' + j % 26;
        }
        texto[n] = '\0';
        ok &= (uart_write_str(texto) == UART0_OK);
    }

    // Sin demoras entre llamadas: comprueba el rearme de START y el orden de envio.
    for (unsigned int i = 0; i < 16; i++) {
        ok &= (uart_write_byte('A' + i) == UART0_OK);
        ok &= (uart_write_str("xy") == UART0_OK);
    }
    ok &= (uart_init() == UART0_OK);
    ok &= (uart_write_str("init\n") == UART0_OK);

    if (ok) {
        ok &= (uart_write_str("[UART0 TEST PASS]\n") == UART0_OK);
    } else {
        uart_write_str("[UART0 TEST FAIL]\n");
    }
    REG_FW_STATUS = ok ? 0xFF : 0xBAD;
}
