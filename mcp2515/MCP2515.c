#include "MCP2515.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "soc/gpio_struct.h"

#include "esp_log.h"

#include "spi_routine.h"

#include "driver/gpio.h"
#include "esp_err.h"

#define REG_BFPCTRL                0x0c
#define REG_TXRTSCTRL              0x0d

#define REG_CANCTRL                0x0f

#define REG_CNF3                   0x28
#define REG_CNF2                   0x29
#define REG_CNF1                   0x2a

#define REG_CANINTE                0x2b
#define REG_CANINTF                0x2c

#define FLAG_RXnIE(n)              (0x01 << n)
#define FLAG_RXnIF(n)              (0x01 << n)
#define FLAG_TXnIF(n)              (0x04 << n)

#define REG_RXFnSIDH(n)            (0x00 + (n * 4))
#define REG_RXFnSIDL(n)            (0x01 + (n * 4))
#define REG_RXFnEID8(n)            (0x02 + (n * 4))
#define REG_RXFnEID0(n)            (0x03 + (n * 4))

#define REG_RXMnSIDH(n)            (0x20 + (n * 0x04))
#define REG_RXMnSIDL(n)            (0x21 + (n * 0x04))
#define REG_RXMnEID8(n)            (0x22 + (n * 0x04))
#define REG_RXMnEID0(n)            (0x23 + (n * 0x04))

#define REG_TXBnCTRL(n)            (0x30 + (n * 0x10))
#define REG_TXBnSIDH(n)            (0x31 + (n * 0x10))
#define REG_TXBnSIDL(n)            (0x32 + (n * 0x10))
#define REG_TXBnEID8(n)            (0x33 + (n * 0x10))
#define REG_TXBnEID0(n)            (0x34 + (n * 0x10))
#define REG_TXBnDLC(n)             (0x35 + (n * 0x10))
#define REG_TXBnD0(n)              (0x36 + (n * 0x10))

#define REG_RXBnCTRL(n)            (0x60 + (n * 0x10))
#define REG_RXBnSIDH(n)            (0x61 + (n * 0x10))
#define REG_RXBnSIDL(n)            (0x62 + (n * 0x10))
#define REG_RXBnEID8(n)            (0x63 + (n * 0x10))
#define REG_RXBnEID0(n)            (0x64 + (n * 0x10))
#define REG_RXBnDLC(n)             (0x65 + (n * 0x10))
#define REG_RXBnD0(n)              (0x66 + (n * 0x10))

#define FLAG_IDE                   0x08
#define FLAG_SRR                   0x10
#define FLAG_RTR                   0x40
#define FLAG_EXIDE                 0x08

#define FLAG_RXM0                  0x20
#define FLAG_RXM1                  0x40

#define TXB_ABTF    0x40
#define TXB_MLOA    0x20
#define TXB_TXERR   0x10
#define TXB_TXREQ   0x08
#define TXB_TXIE    0x04
#define TXB_TXP     0x03

extern xQueueHandle can_frame_queue;
extern xQueueHandle can_irq_quee;

#define MCP "MCP"

const struct {
	long clockFrequency;
	long baudRate;
	uint8_t cnf[3];
} CNF_MAPPER[] = {
	{  (long)8E6, (long)1000E3, { 0x00, 0x80, 0x00 } },
	{  (long)8E6,  (long)500E3, { 0x00, 0x90, 0x02 } },
	{  (long)8E6,  (long)250E3, { 0x00, 0xb1, 0x05 } },
	{  (long)8E6,  (long)200E3, { 0x00, 0xb4, 0x06 } },
	{  (long)8E6,  (long)125E3, { 0x01, 0xb1, 0x05 } },
	{  (long)8E6,  (long)100E3, { 0x01, 0xb4, 0x06 } },
	{  (long)8E6,   (long)80E3, { 0x01, 0xbf, 0x07 } },
	{  (long)8E6,   (long)50E3, { 0x03, 0xb4, 0x06 } },
	{  (long)8E6,   (long)40E3, { 0x03, 0xbf, 0x07 } },
	{  (long)8E6,   (long)20E3, { 0x07, 0xbf, 0x07 } },
	{  (long)8E6,   (long)10E3, { 0x0f, 0xbf, 0x07 } },
	{  (long)8E6,    (long)5E3, { 0x1f, 0xbf, 0x07 } },
};

uint8_t data = 0xC0;
char inited = 0;
bool has_irq = false;

SemaphoreHandle_t mtx;

void(*callback)(struct CanFrame *) = NULL;

void gpio_isr_handler(void* arg) {
	xQueueSendFromISR(can_irq_quee, &data, NULL);
}

void set_cb(void(*cb)(struct CanFrame *)) {
	callback = cb;
}

void remove_cb() {
	callback = NULL;
}

void log_rcv(void* arg) {
	struct CanFrame *fp = NULL;
	for(;;) {
		if(xQueueReceive(can_frame_queue, &fp, 1/portTICK_PERIOD_MS)) {
			if (callback != NULL) {
				callback(fp);
			}
			free(fp);
		}
    	taskYIELD ();
	}
}

void read_frame_tsk() {
	struct CanFrame *io_num = NULL;
	for(;;) {
		if(xQueueReceive(can_irq_quee, &io_num, 1/portTICK_PERIOD_MS)) {
			uint8_t taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);

			while(taken != 1) {
				taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);
			}

			int n = 0;

			if ( read_reg(REG_CANINTF) != 0) {
				uint8_t intf = read_reg(REG_CANINTF);
				if (intf & FLAG_RXnIF(0)) {
					n = 0;
				} else if (intf & FLAG_RXnIF(1)) {
					n = 1;
				}

				long int id = -1;
				bool isExt = (read_reg(REG_RXBnSIDL(n)) & FLAG_IDE) ? true : false;
				int dlc = 0;
				uint8_t data[8];
				memset(&data, 0, sizeof(data));
				uint32_t idA = ((read_reg(REG_RXBnSIDH(n)) << 3) & 0x07f8) | ((read_reg(REG_RXBnSIDL(n)) >> 5) & 0x07);
				if (isExt) {
					uint32_t idB = (((uint32_t)(read_reg(REG_RXBnSIDL(n)) & 0x03) << 16) & 0x30000) | ((read_reg(REG_RXBnEID8(n)) << 8) & 0xff00) | read_reg(REG_RXBnEID0(n));
					id = (idA << 18) | idB;
				} else {
					id = idA;
				}
				dlc = read_reg(REG_RXBnDLC(n)) & 0x0f;

				for (int i = 0; i < dlc; i++) {
					data[i] = read_reg(REG_RXBnD0(n) + i);
				}

				struct CanFrame *frame;
				frame = malloc(sizeof(struct CanFrame));
				if (frame != NULL) {
					frame->IsExt = isExt;
					frame->CanId = id;
					frame->DLC = dlc;
					memcpy(&frame->Data, &data, sizeof(data));
					xQueueSend(can_frame_queue, &frame, 10);
				}
				mod_register(REG_CANINTF, FLAG_RXnIF(n), 0x00);
			}
			has_irq = false;
			xSemaphoreGive(mtx);
		}
    	taskYIELD ();
	}
}

void init_interrupt_handler() {
	gpio_intr_disable(CAN_INT);
	gpio_pulldown_en(CAN_INT);
	gpio_set_intr_type(CAN_INT, GPIO_INTR_NEGEDGE);
	gpio_install_isr_service(( ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LOWMED));
	gpio_isr_handler_add(CAN_INT, &gpio_isr_handler, (void*) CAN_INT);
}

void init_rcv_task() {
	xTaskCreate(&read_frame_tsk, "read_frame", 2048, NULL,  10, NULL);
	xTaskCreate(&log_rcv, "log_rcv", 2048, NULL,  10, NULL);
}

int set_mode(uint8_t mode) {
	uint8_t taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);

	while(taken != 1) {
		taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);
	}

	write_register(REG_CANCTRL, mode);
	if (read_reg(REG_CANCTRL) != mode) {
		return 1;
	}

	xSemaphoreGive(mtx);
	return 0;
}


int disable_mcp() {
	if(set_mode(0x80)) {
		ESP_LOGE(MCP, "Could not disable");
		return 1;
	}
	return 0;
}

int enable_mcp() {
	if(set_mode(0x0)) {
		ESP_LOGE(MCP, "Could not start usual mode");
		return 1;
	}
	return 0;
}

int loopback_mcp() {
	if(set_mode(0x40)) {
		ESP_LOGE(MCP, "Could not start loopback");
		return 1;
	}
	return 0;
}

int set_speed(long baudRate) {
	const uint8_t* cnf = NULL;

	for (unsigned int i = 0; i < (sizeof(CNF_MAPPER) / sizeof(CNF_MAPPER[0])); i++) {
		if (CNF_MAPPER[i].baudRate == baudRate) {
			cnf = CNF_MAPPER[i].cnf;
			break;
		}
	}

	if (cnf == NULL) {
		return 1;
	}
	write_register(REG_CNF1, cnf[0]);
	write_register(REG_CNF2, cnf[1]);
	write_register(REG_CNF3, cnf[2]);
	return 0;
}

void reset_mcp() {
	uint8_t taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);

	while(taken != 1) {
		taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);
	}

	send_data(&data, 1);

	xSemaphoreGive(mtx);
}

void init_once() {
	if (!inited) {
		mtx = xSemaphoreCreateMutex();
		init_rcv_task();
		init_interrupt_handler();
		gpio_intr_enable(CAN_INT);
	}
	inited = 1;
}

void bootstrap_mcp() {
	init_once();
};

esp_err_t init_mcp(long baudRate) {
	esp_err_t err;
	init_once();

	uint8_t taken = xSemaphoreTake(mtx, portMAX_DELAY);

	while(taken != 1) {
		taken = xSemaphoreTake(mtx, portMAX_DELAY);
	}

	err = send_data(&data, 1);
	if (err) {
		ESP_LOGE(MCP, "Send 0xc0 err");
		return err;
	}

	vTaskDelay(100 / portTICK_PERIOD_MS);

	write_register(REG_CANCTRL, 0x80);
	err = read_reg(REG_CANCTRL) != 0x80;
	if (err) {
		ESP_LOGE(MCP, "write_reg REG_CANCTRL err");
		return err;
	}

	err = set_speed(baudRate);
	if (err) {
		ESP_LOGE(MCP, "set speed err");
		return err;
	};

	write_register(REG_CANINTE, FLAG_RXnIE(1) | FLAG_RXnIE(0));
	write_register(REG_BFPCTRL, 0x00);
	write_register(REG_TXRTSCTRL, 0x00);
	write_register(REG_RXBnCTRL(0), FLAG_RXM1 | FLAG_RXM0);
	write_register(REG_RXBnCTRL(1), FLAG_RXM1 | FLAG_RXM0);

	ESP_LOGI(MCP, "finish init");
	xSemaphoreGive(mtx);
	return 0;
}


int _send_frame(struct CanFrame *frame, uint8_t n) {
	if (frame->IsExt) {
		write_register(REG_TXBnSIDH(n), frame->CanId >> 21);
		write_register(REG_TXBnSIDL(n), (((frame->CanId >> 18) & 0x07) << 5) | FLAG_EXIDE | ((frame->CanId >> 16) & 0x03));
		write_register(REG_TXBnEID8(n), (frame->CanId >> 8) & 0xff);
		write_register(REG_TXBnEID0(n), frame->CanId & 0xff);
	} else {
		write_register(REG_TXBnSIDH(n), frame->CanId >> 3);
		write_register(REG_TXBnSIDL(n), frame->CanId << 5);
		write_register(REG_TXBnEID8(n), 0x00);
		write_register(REG_TXBnEID0(n), 0x00);
	}
	write_register(REG_TXBnDLC(n), frame->DLC);

	for (int i = 0; i < frame->DLC; i++) {
		write_register(REG_TXBnD0(n) + i, frame->Data[i]);
	}

	write_register(REG_TXBnCTRL(n), TXB_TXREQ);
	return 0;
}

int send_frame(struct CanFrame *frame) {
	uint8_t taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);

	while(taken != 1) {
		taken = xSemaphoreTake(mtx, 1/portTICK_PERIOD_MS);
	}

	int result = 1;

	for (int i = 0; i < 3; i++) {
		if ((read_reg(REG_TXBnCTRL(i)) & TXB_TXREQ) == 0) {
			ESP_LOGI(MCP, "USE %d buffer", i);
			result = _send_frame(frame, i);
			break;
		}
	}
	if (result) {
		ESP_LOGE(MCP, "No empty buffer");
	}


	xSemaphoreGive(mtx);
	return result;
}