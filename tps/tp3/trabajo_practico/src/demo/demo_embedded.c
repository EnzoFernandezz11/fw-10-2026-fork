
#include "demo.h"
#include "hal_ring.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include "print.h"
#include "strutils.h"

void demo_embedded(void)
{
    uart_regs_t uart = {0};
    uart_init(&uart, 115200);
    uart_send(&uart, (const uint8_t *)"OK", 2);
    print_str("uart BRR=");
    print_dec(uart.BRR);
    print_str(" SR=0x");
    print_hex(uart.SR, 8);
    print_str(" DR=0x");
    print_hex(uart.DR, 8);
    print_str("\n");

    ring_buf_t ring;
    ring_init(&ring);
    for (uint8_t ch = 'A'; ch <= 'J'; ch++)
        ring_push(&ring, ch);
    print_str("ring: ");
    uint8_t ch;
    while (ring_pop(&ring, &ch) == 0)
        print_chr((char)ch);
    print_str("\n");

    uint8_t frame[] = {0xAA, 0x01, 0x02, 0x03, 0x04, 0};
    uint8_t received[sizeof(frame)];

    /* El CRC cubre los cuatro bytes del payload. */
    frame[5] = str_crc8(&frame[1], 4);
    ring_init(&ring);
    for (size_t i = 0; i < sizeof(frame); i++)
        ring_push(&ring, frame[i]);
    for (size_t i = 0; i < sizeof(received); i++)
        ring_pop(&ring, &received[i]);

    if (received[0] == 0xAA && str_crc8(&received[1], 4) == received[5]) {
        print_str("frame: ok crc=0x");
        print_hex(received[5], 2);
        print_str("\n");
    } else {
        print_str("frame: error\n");
    }

    timer_init();
    timer_tick();
    timer_tick();
    timer_tick();
    print_str("timer_ms: ");
    print_dec(timer_get_ms());
    print_str("\n");
}
