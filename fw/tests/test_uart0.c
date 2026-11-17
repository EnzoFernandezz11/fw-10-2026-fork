#include "config.h"
#include "uart0.h"

void main(void) {
    int ok = 1;
    REG_FW_STATUS = 0;

    ok &= (uart_init(UART0) == UART0_OK);
    ok &= (uart_write_str(UART0, "[UART0 TEST BEGIN]\n") == UART0_OK);
    ok &= (uart_write_str(UART0, "Debug UART0\n") == UART0_OK);

    // Los errores y el string vacio no deben transmitir caracteres.
    const uart_id_t invalidos[] = {(uart_id_t)-1, (uart_id_t)1, (uart_id_t)255};
    for (unsigned int i = 0; i < 3; i++) {
        ok &= (uart_init(invalidos[i]) == UART0_ERR);
        ok &= (uart_write_byte(invalidos[i], '!') == UART0_ERR);
        ok &= (uart_write_str(invalidos[i], "NO DEBE SALIR") == UART0_ERR);
        ok &= (uart_write_str(invalidos[i], "") == UART0_ERR);
    }
    ok &= (uart_write_str(UART0, 0) == UART0_ERR);
    ok &= (uart_write_str(UART0, "") == UART0_OK);

    // Incluye byte nulo y bytes cuyo bit de signo esta encendido.
    const uint8_t bytes[] = {0x00, 0x7F, 0x80, 0xFF};
    for (unsigned int i = 0; i < 4; i++) {
        ok &= (uart_write_byte(UART0, (char)bytes[i]) == UART0_OK);
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
        ok &= (uart_write_str(UART0, texto) == UART0_OK);
    }

    // Sin demoras entre llamadas: comprueba el rearme de START y el orden de envio.
    for (unsigned int i = 0; i < 16; i++) {
        ok &= (uart_write_byte(UART0, 'A' + i) == UART0_OK);
        ok &= (uart_write_str(UART0, "xy") == UART0_OK);
    }
    ok &= (uart_init(UART0) == UART0_OK);
    ok &= (uart_write_str(UART0, "init\n") == UART0_OK);

    if (ok) {
        ok &= (uart_write_str(UART0, "[UART0 TEST PASS]\n") == UART0_OK);
    } else {
        uart_write_str(UART0, "[UART0 TEST FAIL]\n");
    }
    REG_FW_STATUS = ok ? 0xFF : 0xBAD;
}
