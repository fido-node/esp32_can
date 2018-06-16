#include "sl_can_proto.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "MCP2515.h"

#define MAIN_TAG "SL_CAN_PROTO"

extern xQueueHandle cdc_response_queue;

long baud = (long)100E3;

char* SLCAN_VERSION =  "V0101\r";
char* SLCAN_DEV =  "vSTM32\r";
char* OK_R = "\r";

void send_to_cdc(char *resp, uint32_t len) {
	struct CDCResponse *data = NULL;
	data = malloc(sizeof(struct CDCResponse));
	heap_caps_check_integrity_all(true);
	if (data != NULL) {
		data->response = resp;
		data->length = len;
		xQueueSend(cdc_response_queue, &data, 10);
	}
}

void frame_cb(struct CanFrame *f) {
	ESP_LOGI(MAIN_TAG, "FRAME");
	if (!f->IsExt) {
		uint8_t char_len = 6;
		char_len = char_len + (f->DLC * 2) + 1;

		char* str = NULL;
		ESP_LOGI(MAIN_TAG, "NON %d", char_len);

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
			ESP_LOGI(MAIN_TAG, "%s", str);
			send_to_cdc(str, char_len);
			// heap_caps_check_integrity_all(true);
			// free(str);
		}

	} else {
		ESP_LOGI(MAIN_TAG, "EXT?!?!?!");
	}
}

void init_sl_can() {
	set_cb(&frame_cb);
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

void sl_send_frame(uint8_t *cmd, uint32_t len) {
	struct CanFrame frame;
	frame.IsExt = false;
	frame.CanId = 10;
	frame.DLC = 5;
	frame.Data[0] = 0xDE;
	frame.Data[1] = 0xAD;
	frame.Data[2] = 0x00;
	frame.Data[3] = 0xBE;
	frame.Data[4] = 0xAF;

	send_frame(&frame);
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
		sl_set_speed(cmd[1]);
		send_to_cdc(OK_R, 1);
		break;
		case 'O':
		ESP_LOGI(MAIN_TAG, "O");
		init_mcp(baud);
		// enable_mcp();
		loopback_mcp();
		send_to_cdc(OK_R, 1);
		break;
		case 'L':
		ESP_LOGI(MAIN_TAG, "L");
		loopback_mcp();
		send_to_cdc(OK_R, 1);
		break;
		case 'C':
		ESP_LOGI(MAIN_TAG, "C");
		reset_mcp();
		send_to_cdc(OK_R, 1);
		break;
		case 't':
		ESP_LOGI(MAIN_TAG, "t");
		sl_send_frame(cmd, len);
		send_to_cdc(OK_R, 1);
		break;
		default:
		send_to_cdc(OK_R, 1);
	}
}

