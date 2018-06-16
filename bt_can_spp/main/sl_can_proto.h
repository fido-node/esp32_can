#ifndef SL_CAN_PROTO_H__
#define SL_CAN_PROTO_H__

#include <stdbool.h>
#include <string.h>
#include "esp_err.h"

typedef struct CDCResponse {
  uint32_t length;
  char *response;
} cdc_response;


void receive_cmd(uint8_t *cmd, uint32_t len);
void init_sl_can();

#endif