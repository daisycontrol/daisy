#pragma once
#include "tim.h"

#define STATE_CHANGE_ID 1

#define READ_MEMORY_ID 100
#define WRITE_MEMORY_ID 101

#ifdef __cplusplus
extern "C" {
#endif

void app_main(void);
// void tim_elapsed_callback(TIM_HandleTypeDef*);
// void gpio_interrupt_callback(uint16_t pin);
void usb_callback(uint8_t*, uint32_t);

#ifdef __cplusplus
}
#endif
