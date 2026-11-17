#include "includes.h"
#include "led.h"

void main(void) {
    int ok = 1; // Cualquier comparacion falsa lo deja en 0 hasta el final.
    REG_FW_STATUS = 0;

    led_init();
    ok &= (led_get_mask() == 0x00);

    // Encender, apagar y alternar un solo LED.
    led_on(3);
    ok &= (led_get_mask() == 0x08);
    ok &= (led_get_status(3) == 1);

    led_off(3);
    ok &= (led_get_mask() == 0x00);
    ok &= (led_get_status(3) == 0);

    led_toggle(3);
    ok &= (led_get_mask() == 0x08);
    led_toggle(3);
    ok &= (led_get_mask() == 0x00);

    print_str(ok ? "-- LED: un bit PASS --" : "-- LED: un bit FAIL --");

    // Primer y ultimo indice valido.
    const uint8_t extremos[] = {0, 7};
    for (unsigned int i = 0; i < 2; i++) {
        uint8_t idx = extremos[i];
        led_write(0);
        led_on(idx);
        ok &= (led_get_mask() == (1u << idx));
        ok &= (led_get_status(idx) == 1);
        led_off(idx);
        ok &= (led_get_mask() == 0);
        ok &= (led_get_status(idx) == 0);
        led_toggle(idx);
        ok &= (led_get_mask() == (1u << idx));
        led_toggle(idx);
        ok &= (led_get_mask() == 0);
    }

    // Valores extremos y descarte de los bits superiores de la mascara.
    led_write(0xFF);
    ok &= (led_get_mask() == 0xFF);
    led_write(0x00);
    ok &= (led_get_mask() == 0x00);
    led_set_mask(0xFFFFFFFFu);
    ok &= (led_get_mask() == 0xFF);
    led_set_mask(0xFFFFFF00u);
    ok &= (led_get_mask() == 0x00);

    // Los indices invalidos no deben modificar el estado.
    const uint8_t invalidos[] = {8, 255};
    led_write(0xA5);
    for (unsigned int i = 0; i < 2; i++) {
        uint8_t idx = invalidos[i];
        led_on(idx);
        ok &= (led_get_mask() == 0xA5);
        led_off(idx);
        ok &= (led_get_mask() == 0xA5);
        led_toggle(idx);
        ok &= (led_get_mask() == 0xA5);
        ok &= (led_get_status(idx) == 255);
        ok &= (led_get_mask() == 0xA5);
    }

    // Inicializar otra vez debe apagar los LED que estaban encendidos.
    led_init();
    ok &= (led_get_mask() == 0x00);

    // Encender varios LED y comprobar que las operaciones preservan los otros bits.
    led_on(0);
    led_on(3);
    led_on(7);
    ok &= (led_get_mask() == 0x89);

    led_off(0);
    led_off(7);
    ok &= (led_get_mask() == 0x08);

    led_toggle(3);
    led_toggle(6);
    ok &= (led_get_mask() == 0x40);

    if (ok) {
        print_str("-- TEST LED PASS: estado final 0x40 --");
        REG_FW_STATUS = 0xFF;
    } else {
        print_str("-- TEST LED FAIL --");
        REG_FW_STATUS = 0xBAD;
    }
}
