#ifndef MCP2515_H
#define MCP2515_H
#include "esp_err.h"
#include "can.h"

#define LED_TX 23
#define LED_RX 22
#define CAN_INT 17

esp_err_t init_mcp(long baudRate);
esp_err_t send_frame(can_frame_tx_t *frame);
int loopback_mcp();
int disable_mcp();
int enable_mcp();
void set_cb(void(*cb)(can_frame_tx_t *));
void remove_cb();
#endif