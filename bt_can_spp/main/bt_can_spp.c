#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "esp_system.h"
#include "esp_event_loop.h"


#include "time.h"
#include "sys/time.h"

#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include "shared_gpio.h"
#include "spi_routine.h"
#include "MCP2515.h"
#include "can.h"

#define MAIN_TAG "MAIN"
#define SPP_TAG "SPP_ACCEPTOR"
#define SPP_SERVER_NAME "SPP_SERVER"
#define EXAMPLE_DEVICE_NAME "CAN_ESP_IF"
#define SPP_SHOW_DATA 0
#define SPP_SHOW_SPEED 1
#define SPP_SHOW_MODE SPP_SHOW_SPEED

xQueueHandle can_evt_queue = NULL;
xQueueHandle can_frame_queue = NULL;


static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;

static struct timeval time_new, time_old;
static long data_num = 0;

static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;


static void print_speed(void) {
	float time_old_s = time_old.tv_sec + time_old.tv_usec / 1000000.0;
	float time_new_s = time_new.tv_sec + time_new.tv_usec / 1000000.0;
	float time_interval = time_new_s - time_old_s;
	float speed = data_num * 8 / time_interval / 1000.0;
	ESP_LOGI(SPP_TAG, "speed(%fs ~ %fs): %f kbit/s" , time_old_s, time_new_s, speed);
	data_num = 0;
	time_old.tv_sec = time_new.tv_sec;
	time_old.tv_usec = time_new.tv_usec;
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
	switch (event) {
		case ESP_SPP_INIT_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
			esp_bt_dev_set_device_name(EXAMPLE_DEVICE_NAME);
			esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
			esp_spp_start_srv(sec_mask,role_slave, 0, SPP_SERVER_NAME);
			break;
		case ESP_SPP_DISCOVERY_COMP_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
			break;
		case ESP_SPP_OPEN_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
			break;
		case ESP_SPP_CLOSE_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
			break;
		case ESP_SPP_START_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
			break;
		case ESP_SPP_CL_INIT_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
			break;
		case ESP_SPP_DATA_IND_EVT:
	#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
			ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d",
					 param->data_ind.len, param->data_ind.handle);
			esp_log_buffer_hex("",param->data_ind.data,param->data_ind.len);
	#else
			gettimeofday(&time_new, NULL);
			data_num += param->data_ind.len;
			if (time_new.tv_sec - time_old.tv_sec >= 3) {
				print_speed();
			}
	#endif
			break;
		case ESP_SPP_CONG_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
			break;
		case ESP_SPP_WRITE_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
			break;
		case ESP_SPP_SRV_OPEN_EVT:
			ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
			gettimeofday(&time_old, NULL);
			break;
		default:
			break;
	}
}

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

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK( ret );


	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bluedroid_init()) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_bluedroid_enable()) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

	if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK) {
		ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
		return;
	}

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

	// test_can_routine();

	ret = enable_mcp();
	if (ret) {
		ESP_LOGE(MAIN_TAG, "enable mcp fail");
		return;
	}

}

