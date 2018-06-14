#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "shared_gpio.h"
#include "spi_routine.h"
#include "MCP2515.h"
#include "can.h"


#define MAIN_TAG "MAIN"

xQueueHandle can_evt_queue = NULL;
xQueueHandle can_frame_queue = NULL;

void send_can(void* arg) {
	can_frame_tx_t frame = {
		.IsExt = true,
		.CanId = 1,
		.DLC = 5,
		.Data = {0xDE, 0xAD, 0x00, 0xBE, 0xAF},
	};

	for(;;) {
		ESP_LOGI(MAIN_TAG, "SND CAN FRM");
		send_frame(&frame);
		vTaskDelay(500/portTICK_PERIOD_MS);
	}
}

void test_can_routine() {
	loopback_mcp();
	xTaskCreate(&send_can, "can_send_task", 2048, NULL, 10, NULL);
}

void frame_cb(can_frame_tx_t *f) {
	ESP_LOGI(MAIN_TAG, "ID: %li, DLC: %d, [%x,%x,%x,%x,%x,%x,%x,%x]",
				f->CanId,
				f->DLC,
				f->Data[0],
				f->Data[1],
				f->Data[2],
				f->Data[3],
				f->Data[4],
				f->Data[5],
				f->Data[6],
				f->Data[7]);
}

void app_main() {
	esp_err_t ret;
	can_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	can_frame_queue = xQueueCreate(10, sizeof(uint32_t));
	init_gpio();

	set_cb(&frame_cb);

	ret = init_spi();
	if (ret) {
		ESP_LOGE(MAIN_TAG, "%s initialize spi failed\n", __func__);
		return;
	}


	ret = init_mcp((long) 500E3);
	if (ret) {
		ESP_LOGE(MAIN_TAG, "%s initialize mcp failed\n", __func__);
		return;
	}

	test_can_routine();

	// ret = enable_mcp();
	// if (ret) {
		// ESP_LOGE(MAIN_TAG, "enable mcp fail");
		// return;
	// }

}

