#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "config.h"

#define LED_OK             0u
#define LED_ERR_IDX        1u
#define LED_INVALID_STATUS 0xFFu

void led_init(void);
uint8_t led_on(uint8_t idx);
uint8_t led_off(uint8_t idx);
uint8_t led_toggle(uint8_t idx);
void led_write(uint8_t value);
void led_set_mask(uint32_t mask);
uint8_t led_get_mask(void);
uint8_t led_get_status(uint8_t idx);

#endif
