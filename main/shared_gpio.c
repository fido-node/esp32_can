#include "shared_gpio.h"
#include "driver/gpio.h"

void init_gpio() {
	gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

void tgl_led() {
	gpio_set_level(GPIO_LED, 1 - gpio_get_level(GPIO_LED));
}

void led_on() {
	gpio_set_level(GPIO_LED, 1);
}

void led_off() {
	gpio_set_level(GPIO_LED, 0);
}
