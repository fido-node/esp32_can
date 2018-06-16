#ifndef MCP2515_H
#define MCP2515_H
#include "esp_err.h"
#include "can.h"

#define CAN_INT 17

esp_err_t init_mcp(long baudRate);
esp_err_t send_frame(struct CanFrame *frame);
int loopback_mcp();
int disable_mcp();
int enable_mcp();
void reset_mcp();
void set_cb(void(*cb)(struct CanFrame *));
void remove_cb();

void bootstrap_mcp();
#endif