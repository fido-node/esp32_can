
#include "MCP2515.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "soc/gpio_struct.h"

#include "esp_log.h"

#include "spi_routine.h"
#include "shared_gpio.h"

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

extern xQueueHandle can_evt_queue;

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

SemaphoreHandle_t mtx;

void gpio_isr_handler(void* arg) {
	xQueueSendFromISR(can_evt_queue, &data, NULL);
}

void read_frame() {
	uint8_t io_num;
	for(;;) {
		led_off();
		if(xQueueReceive(can_evt_queue, &io_num, 1)) {
			xSemaphoreTake(mtx, 100000);
			led_on();
			ESP_LOGI(MCP, "ISR CAN RCV");
			int n = 0;
			uint8_t response[4];
			read_register(REG_CANINTF, &response);
			if ( response[2] != 0) {
				uint8_t intf = response[2];
				ESP_LOGI(MCP, "CAN BF: %d", intf);

				if (intf & FLAG_RXnIF(0)) {
					n = 0;
					// modify_register(REG_CANINTF, FLAG_RXnIF(n), 0x00);
				} else if (intf & FLAG_RXnIF(1)) {
					n = 1;
				}
				mod_register(REG_CANINTF, FLAG_RXnIF(n), 0x00);
			}
		xSemaphoreGive(mtx);
		}
	}
}


esp_err_t init_mcp(long baudRate) {
	mtx = xSemaphoreCreateMutex();
	esp_err_t err;

	err = send_data(&data, 1);
	ESP_LOGI(MCP, "Send 0xc0");
	if (err) {
		ESP_LOGE(MCP, "Send 0xc0 err");
		return err;
	}

	vTaskDelay(100);
	
	err = write_register(REG_CANCTRL, 0x80);
	ESP_LOGI(MCP, "write_reg REG_CANCTRL");
	if (err) {
		ESP_LOGE(MCP, "write_reg REG_CANCTRL err");
		return err;
	}

	uint8_t response[4];

	err = read_register(REG_CANCTRL, &response);
	if (err) {
		ESP_LOGE(MCP, "read_register REG_CANCTRL err");
		return err;
	}
	ESP_LOGI(MCP, "response %d, %d, %d, %d", response[0], response[1], response[2], response[3]);


	if (response[2] != 0x80) {
		return 1;
	};
	led_on();

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

	ESP_LOGI(MCP, "response set speed");
	write_register(REG_CNF1, cnf[0]);
	write_register(REG_CNF2, cnf[1]);
	write_register(REG_CNF3, cnf[2]);

	ESP_LOGI(MCP, "response set int");
	write_register(REG_CANINTE, FLAG_RXnIE(1) | FLAG_RXnIE(0));
	write_register(REG_BFPCTRL, 0x00);
	write_register(REG_TXRTSCTRL, 0x00);
	write_register(REG_RXBnCTRL(0), FLAG_RXM1 | FLAG_RXM0);
	write_register(REG_RXBnCTRL(1), FLAG_RXM1 | FLAG_RXM0);


	ESP_LOGI(MCP, "enable receive");
	// write_register(REG_CANCTRL, 0x40);
	write_register(REG_CANCTRL, 0x0);
	read_register(REG_CANCTRL, &response);
	// if (response[2] != 0x40) {
	if (response[2] != 0x00) {
		return 1;
	}


	ESP_LOGI(MCP, "finish init");
	led_off();

	xTaskCreate(&read_frame, "read_frame", 8192, NULL, 10, NULL);

	//change gpio intrrupt type for one pin
	gpio_set_intr_type(CAN_INT, GPIO_INTR_LOW_LEVEL);

    //install gpio isr service
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin again
	gpio_isr_handler_add(CAN_INT, gpio_isr_handler, (void*) CAN_INT);
	
	return 0;
}

int send_frame(can_frame_tx_t *frame) {
	xSemaphoreTake(mtx, 10000);
	int n = 0;
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

	write_register(REG_TXBnCTRL(n), 0x08);

	uint8_t response[4];
	read_register(REG_TXBnCTRL(n), &response);

	while (response[2] & 0x08) {
		uint8_t  tx_read[4];
		read_register(REG_TXBnCTRL(n), &tx_read);
		if (tx_read[2] & 0x10) {
			mod_register(REG_CANCTRL, 0x10, 0x10);
		}
		vTaskDelay(1);
		read_register(REG_TXBnCTRL(n), &response);
	}
	mod_register(REG_CANINTF, FLAG_TXnIF(n), 0x00);
	read_register(REG_TXBnCTRL(n), &response);
	xSemaphoreGive(mtx);
	return (response[2] & 0x70) ? 0 : 1;
}