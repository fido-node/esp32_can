#include "spi_routine.h"
#include <string.h>
#include "esp_system.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   15

static spi_device_handle_t spi;

esp_err_t init_spi() {
	esp_err_t ret;

	spi_bus_config_t buscfg={
		.miso_io_num=PIN_NUM_MISO,
		.mosi_io_num=PIN_NUM_MOSI,
		.sclk_io_num=PIN_NUM_CLK,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
		.max_transfer_sz=4096
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=10*1000*1000,           //Clock out at 10 MHz
		.mode=0,                                //SPI mode 0
		.spics_io_num=PIN_NUM_CS,               //CS pin
		.queue_size=7,
		// .pre_cb=lcd_spi_pre_transfer_callback,
	};

		//Initialize the SPI bus
	ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
	ESP_ERROR_CHECK(ret);
	//Attach the LCD to the SPI bus
	ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
	ESP_ERROR_CHECK(ret);
	return ret;
}

esp_err_t write_register(uint8_t addr, uint8_t val) {
	esp_err_t ret;
	spi_transaction_t t;

	uint8_t data[3];
	data[0]=0x02;
	data[1]=addr;
	data[2]=val;

	memset(&t, 0, sizeof(t));
	t.length=24;
	t.tx_buffer=&data;
	ret=spi_device_transmit(spi, &t);
	return ret;
}

esp_err_t mod_register(uint8_t addr, uint8_t mask, uint8_t val) {
	spi_transaction_t t;
	esp_err_t ret;

	uint8_t data[4];
	data[0]=0x05;
	data[1]=addr;
	data[2]=mask;
	data[3]=val;

	memset(&t, 0, sizeof(t));
	t.length=32;
	t.tx_buffer=&data;
	ret=spi_device_transmit(spi, &t);
	return ret;
}

uint8_t read_reg(uint8_t addr) {
	spi_transaction_t r;

	uint8_t data[2];
	data[0]=0x03;
	data[1]=addr;

	memset(&r, 0, sizeof(r));
	r.length=8*4;
	r.tx_buffer=&data;
	r.flags = SPI_TRANS_USE_RXDATA;

	spi_device_transmit(spi, &r);
	return r.rx_data[2];
}

esp_err_t send_data(const uint8_t *data, int len) {
	esp_err_t ret;
	spi_transaction_t t;
	if (len==0) return 0;
	memset(&t, 0, sizeof(t));
	t.length=len*8;
	t.tx_buffer=data;
	ret=spi_device_transmit(spi, &t);
	return ret;
}
