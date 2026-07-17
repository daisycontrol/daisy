#include "app_main.h"

#include "hardware/imu/app/inc/leds.h"
#include "main.h"

#include "usbd_cdc_if.h"

#include "tim.h"
#include "imu.h"
#include "leds.h"

#include "common/scheduler/scheduler.h"
#include "common/memory/memory.h"
#include "schema/common_messages.h"

// void tx_fn(const uint8_t* data, size_t len){
//   comms::transmit(data, len);
// }
// ipc::IPCRouter router(tx_fn);



void app_main(void){
    CDC_RegisterRxCallback(usb_callback);

    imu::start_bmi270();
    scheduler::initialize();

    // Set up tasks
    scheduler::PeriodicTask imu_read_task(&htim2);
    imu_read_task.period_us = scheduler::period_from_hertz(200);

    scheduler::PeriodicTask led_task(&htim2);
    led_task.period_us = scheduler::period_from_hertz(200);
  

    while(true){

      if(imu_read_task.tick()){
        imu::update_imu_data();
      }

      if(led_task.tick()){
        led::set_led_float(0, 0.0f, 0.0f, 1.0f);
        led::set_led_float(1, 0.0f, 1.0f, 0.0f);
        led::WS2812_Send();
      }
    }
}

#define CDC_RX_BUFFER_SIZE 128
uint8_t cdcRxBuffer[CDC_RX_BUFFER_SIZE];
uint32_t cdcRxBufferIndex = 0;

void usb_callback(uint8_t *Buf, uint32_t Len){
  for (int i = 0; i < Len; i++) {
    // Check for buffer overflow
    if (cdcRxBufferIndex < CDC_RX_BUFFER_SIZE) {
        cdcRxBuffer[cdcRxBufferIndex++] = Buf[i];
    }

    if(Buf[i] == '\r'){
      // Handle new command
    //   comms::slcan_usb_rx(cdcRxBuffer, cdcRxBufferIndex);
      cdcRxBufferIndex = 0;
    }
  }
}