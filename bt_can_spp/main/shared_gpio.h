#ifndef SHARED_GPIO_H
#define SHARED_GPIO_H
#include "esp_err.h"

#define GPIO_LED    2

#define GPIO_OUTPUT_PIN_SEL  (1<<GPIO_LED)

#define GPIO_INPUT_PIN_SEL  (1ULL<<CAN_INT)
#define ESP_INTR_FLAG_DEFAULT 0

void tgl_led();
void led_on();
void led_off();
void init_gpio();

// void p_on(uint8_t id);
// void p_off(uint8_t id);
// void p_tg(uint8_t id);

#endif