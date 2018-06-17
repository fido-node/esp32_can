#ifndef MCP2515_H
#define MCP2515_H
#include "esp_err.h"
#include "can.h"

#define CAN_INT 17
#define OPEN_MODE  0x00
#define LOOPBACK_MODE 0x40
#define CONFIG_MODE 0x80


esp_err_t init_mcp(long baudRate, int mode);
esp_err_t send_frame(struct CanFrame *frame);

esp_err_t reset_mcp();

void set_cb(void(*cb)(struct CanFrame *));

void remove_cb();

void bootstrap_mcp();

#endif