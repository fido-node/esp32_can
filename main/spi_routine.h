#ifndef SPI_ROUTINE_H
#define SPI_ROUTINE_H
#include "esp_err.h"

esp_err_t init_spi();
esp_err_t send_data(const uint8_t *data, int len);

esp_err_t write_register(uint8_t addr, uint8_t val);
esp_err_t mod_register(uint8_t addr, uint8_t mask, uint8_t val);
esp_err_t read_register(uint8_t addr, uint8_t (*val)[4]);


#endif