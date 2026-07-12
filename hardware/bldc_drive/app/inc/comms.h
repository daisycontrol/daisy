#pragma once

#include "stm32g4xx_hal.h"
#include "main.h"
#include "fdcan.h"
#include "usbd_cdc_if.h"
#include <string>
#include "common/ipc/ipc.h"

namespace comms
{

void initialize(FDCAN_HandleTypeDef*, ipc::IPCRouter*);

void set_device_id(uint16_t);
uint16_t get_device_id();

int can_transmit(FDCAN_TxHeaderTypeDef*,  const uint8_t*);
int transmit(const uint8_t* data, size_t len);
void can_receive(FDCAN_RxHeaderTypeDef*, uint8_t*);

void send_slcan_string(FDCAN_RxHeaderTypeDef, const uint8_t*);
void slcan_usb_rx(uint8_t*, uint32_t);

FDCAN_RxHeaderTypeDef convert_tx_header(FDCAN_TxHeaderTypeDef*);
void handle_can_rx(FDCAN_HandleTypeDef *hfdcan);

};