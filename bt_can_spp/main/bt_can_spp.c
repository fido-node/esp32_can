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
#include "sl_can_proto.h"

#define MAIN_TAG "MAIN"
#define SPP_TAG "SPP_ACCEPTOR"
#define SPP_SERVER_NAME "SPP_SERVER"
#define EXAMPLE_DEVICE_NAME "CAN_ESP_IF"

xQueueHandle can_frame_queue = NULL;
// xQueueHandle cdc_response_queue = NULL;
xQueueHandle can_irq_quee = NULL;

uint32_t handle = 0;

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

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
		reset_mcp();
		ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT");
		break;
		case ESP_SPP_START_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT");
		break;
		case ESP_SPP_CL_INIT_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
		break;
		case ESP_SPP_DATA_IND_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len=%d handle=%d",
			param->data_ind.len, param->data_ind.handle);
		receive_cmd(param->data_ind.data, param->data_ind.len);
		break;
		case ESP_SPP_CONG_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
		break;
		case ESP_SPP_WRITE_EVT:
		ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
		break;
		case ESP_SPP_SRV_OPEN_EVT:
		handle = param->write.handle;
		ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT");
		break;
		default:
		break;
	}
}

void app_main() {
	esp_err_t ret;
	can_frame_queue = xQueueCreate(10, sizeof(uint32_t));
	can_irq_quee = xQueueCreate(10, sizeof(uint32_t));
	// cdc_response_queue = xQueueCreate(10, sizeof(uint32_t));

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);


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
	init_sl_can();

	ret = init_spi();
	if (ret) {
		ESP_LOGE(MAIN_TAG, "%s initialize spi failed\n", __func__);
		return;
	}
	bootstrap_mcp();

	// ret = init_spi();
	// if (ret) {
	// 	ESP_LOGE(MAIN_TAG, "%s initialize spi failed\n", __func__);
	// 	return;
	// }

}

