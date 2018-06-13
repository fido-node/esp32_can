#ifndef SHARED_GPIO_H
#define SHARED_GPIO_H

#define CAN_INT      17

#define GPIO_LED    2
#define GPIO_OUTPUT_PIN_SEL  (1<<GPIO_LED)

#define GPIO_INPUT_PIN_SEL  (1ULL<<CAN_INT)
#define ESP_INTR_FLAG_DEFAULT 0

void tgl_led();
void led_on();
void led_off();
void init_gpio();

#endif