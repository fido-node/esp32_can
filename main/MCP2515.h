#ifndef MCP2515_H
#define MCP2515_H
#include "esp_err.h"
#include "can.h"

esp_err_t init_mcp(long baudRate);
esp_err_t send_frame(can_frame_tx_t *frame);

#endif