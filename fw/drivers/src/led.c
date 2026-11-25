#include "led.h"


void led_init(void) {
    REG_LED = 0x00;
}

uint8_t led_on(uint8_t idx) {
    if (idx >= 8) {
        return LED_ERR_IDX;
    }
    REG_LED |= (1u << idx);
    return LED_OK;
}

uint8_t led_off(uint8_t idx) {
    if (idx >= 8) {
        return LED_ERR_IDX;
    }
    REG_LED &= ~(1u << idx);
    return LED_OK;
}

uint8_t led_toggle(uint8_t idx) {
    if (idx >= 8) {
        return LED_ERR_IDX;
    }
    REG_LED ^= (1u << idx);
    return LED_OK;
}

void led_write(uint8_t value) {
    REG_LED = value;
}

void led_set_mask(uint32_t mask) {
    REG_LED = mask & 0xFF;
}

uint8_t led_get_mask(void) {
    return REG_LED & 0xFF;
}

uint8_t led_get_status(uint8_t idx) {
    if (idx < 8) {
        return (REG_LED >> idx) & 0x01;
    }
    return LED_INVALID_STATUS;
}

