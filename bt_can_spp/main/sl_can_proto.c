#include "sl_can_proto.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_spp_api.h"

#include "MCP2515.h"

#define MAIN_TAG "SL_CAN_PROTO"

// extern xQueueHandle cdc_response_queue;

long baud = (long)100E3;

extern uint32_t handle;

char* SLCAN_VERSION =  "V0101\r";
char* SLCAN_DEV =  "vSTM32\r";
char* OK_R = "\r";


void send_to_cdc(char *resp, uint32_t len) {

	esp_spp_write(handle, len, (uint8_t *)resp);
	ESP_LOGI(MAIN_TAG, "out to %d, bytes:%d", handle, len);
	if (resp != SLCAN_VERSION && resp != SLCAN_DEV &&  resp != OK_R) {
		free(resp);
	}
}

void frame_tx_cb(uint8_t *cmd, uint32_t len) {
	if (len > 6) {
		ESP_LOGI(MAIN_TAG, "cdc2can %s", cmd);

		struct CanFrame *f = malloc(sizeof(struct CanFrame));
		memset(f, 0, sizeof(struct CanFrame));
		if (f != NULL) {
			char* id_s = malloc(4);
			char* dlc_s = malloc(2);
			char* byte_s = malloc(3);
			// t0221FF
			strncpy(id_s, (char *)cmd + 1, 3);
			strncpy(dlc_s, (char *)cmd + 4, 1);
			f->IsExt = false;
			f->CanId =(uint32_t) strtoul((char *) id_s, NULL, 16);
			f->DLC = atoi(dlc_s);
			for (uint8_t i = 0; i < f->DLC; i++) {
				strncpy(byte_s, (char *)cmd + (5 + (2 * i)), 2);
				f->Data[i] = (uint8_t) strtoul((char *) byte_s, NULL, 16);
			}

			free(id_s);
			free(dlc_s);
			free(byte_s);
			send_frame(f);
			free(f);
		}
	}
}

void frame_rx_cb(struct CanFrame *f) {
	if (!f->IsExt) {
		uint8_t char_len = 6;
		char_len = char_len + (f->DLC * 2) + 1;
		char* str = NULL;

		str = malloc(char_len);
		// heap_caps_check_integrity_all(true);
		if (str != NULL) {
			switch (f->DLC) {
				case 0:
				sprintf(str, "t%03x%d\r", f->CanId, f->DLC);
				break;
				case 1:
				sprintf(str, "t%03x%d%02x\r", f->CanId, f->DLC, f->Data[0]);
				break;
				case 2:
				sprintf(str, "t%03x%d%02x%02x\r", f->CanId, f->DLC,
					f->Data[0], f->Data[1]);
				break;
				case 3:
				sprintf(str, "t%03x%d%02x%02x%02x\r", f->CanId, f->DLC,
					f->Data[0], f->Data[1], f->Data[2]);
				break;
				case 4:
				sprintf(str, "t%03x%d%02x%02x%02x%02x\r", f->CanId,
					f->DLC, f->Data[0], f->Data[1], f->Data[2], f->Data[3]);
				break;
				case 5:
				sprintf(str, "t%03x%d%02x%02x%02x%02x%02x\r" , f->CanId,
					f->DLC, f->Data[0], f->Data[1], f->Data[2], f->Data[3],
					f->Data[4]);
				break;
				case 6:
				sprintf(str, "t%03x%d%02x%02x%02x%02x%02x%02x\r", f->CanId,
					f->DLC, f->Data[0], f->Data[1], f->Data[2], f->Data[3],
					f->Data[4], f->Data[5]);
				break;
				case 7:
				sprintf(str, "t%03x%d%02x%02x%02x%02x%02x%02x%02x\r", f->CanId,
					f->DLC, f->Data[0], f->Data[1], f->Data[2], f->Data[3],
					f->Data[4], f->Data[5], f->Data[6]);
				break;
				case 8:
				sprintf(str, "t%03x%d%02x%02x%02x%02x%02x%02x%02x%02x\r", f->CanId,
					f->DLC, f->Data[0], f->Data[1], f->Data[2], f->Data[3],
					f->Data[4], f->Data[5], f->Data[6], f->Data[7]);
				break;
			}
			ESP_LOGI(MAIN_TAG, "can2cdc %s", str);
			send_to_cdc(str, char_len);
		}

	} else {
		ESP_LOGI(MAIN_TAG, "EXT?!?!?!");
	}
}

void init_sl_can() {
	set_cb(&frame_rx_cb);
}

void sl_set_speed(char num) {
	switch (num) {
		case '0':
		baud = (long)10E3;
		break;
		case '1':
		baud = (long)20E3;
		break;
		case '2':
		baud = (long)50E3;
		break;
		case '3':
		baud = (long)100E3;
		break;
		case '4':
		baud = (long)125E3;
		break;
		case '5':
		baud = (long)250E3;
		break;
		case '6':
		baud = (long)500E3;
		break;
		case '7':
		baud = (long)800E3;
		break;
		case '8':
		baud = (long)1000E3;
		break;
	}
}

void receive_cmd(uint8_t *cmd, uint32_t len) {
	switch (cmd[0]) {
		case 'V':
		send_to_cdc(SLCAN_VERSION, 5);
		ESP_LOGI(MAIN_TAG, "V");
		break;
		case 'v':
		ESP_LOGI(MAIN_TAG, "v");
		send_to_cdc(SLCAN_DEV, 7);
		break;
		case 'S':
		ESP_LOGI(MAIN_TAG, "S");
		send_to_cdc(OK_R, 1);
		sl_set_speed(cmd[1]);
		break;
		case 'O':
		ESP_LOGI(MAIN_TAG, "O");
		send_to_cdc(OK_R, 1);
		init_mcp(baud);
		// enable_mcp();
		loopback_mcp();
		break;
		case 'L':
		ESP_LOGI(MAIN_TAG, "L");
		send_to_cdc(OK_R, 1);
		loopback_mcp();
		break;
		case 'C':
		ESP_LOGI(MAIN_TAG, "C");
		send_to_cdc(OK_R, 1);
		reset_mcp();
		break;
		case 't':
		ESP_LOGI(MAIN_TAG, "t");
		send_to_cdc(OK_R, 1);
		frame_tx_cb(cmd, len);
		break;
		default:
		send_to_cdc(OK_R, 1);
	}
}

