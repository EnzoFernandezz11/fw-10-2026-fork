#include <stdint.h>
#include "config.h" 

#define LED_OK      0
#define LED_ERR_IDX 1

void led_init(void);
uint8_t led_on(uint8_t idx);
uint8_t led_off(uint8_t idx);
uint8_t led_toggle(uint8_t idx);
void led_write(uint8_t value);
void led_set_mask(uint32_t mask);
uint8_t led_get_mask(void);
uint8_t led_get_status(uint8_t idx);
