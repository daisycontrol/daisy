#include "app_main.h"
// c lib includes
#include "string.h"
#include <stdio.h>
// ST HAL includes
#include "stm32g4xx_hal.h"
#include "tim.h"
#include "fdcan.h"
#include "usbd_cdc_if.h"
// Custom lib includes
#include "structs.h"
#include "scheduler.h"
#include "analog.h"
#include "drive.h"
#include "encoder.h"
#include "led.h"
#include "trig.h"
#include "foc.h"
#include "uid_hash.h"
#include "comms.h"

#include "common/ipc/ipc.h"

#include "schema/common_messages.h"


// struct ControllerTarget target;
int count = 0;
int angle = 0;

#define CDC_RX_BUFFER_SIZE 128
uint8_t cdcRxBuffer[CDC_RX_BUFFER_SIZE];
uint32_t cdcRxBufferIndex = 0;

void tx_fn(const uint8_t* data, size_t len){
  volatile int x = 1;
  return;
}

void app_main(void){
  CDC_RegisterRxCallback(usb_callback);

  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);

  ipc::IPCRouter router(tx_fn);

  // Some modules need to be re-inited based on drive voltage state
  scheduler::initialize();
  uid_hash::initialize();
  trig::initialize();
  encoder::initialize();
  drive::initialize();
  analog::initialize();
  led::initialize();
  comms::initialize(&hfdcan1);

  scheduler::PeriodicTask blink_task(&htim2);
  blink_task.period_us = scheduler::period_from_hertz(60);

  scheduler::PeriodicTask drive_state_machine(&htim2);
  drive_state_machine.period_us = scheduler::period_from_hertz(1000);

  scheduler::PeriodicTask analog_update_task(&htim2);
  analog_update_task.period_us = scheduler::period_from_hertz(1000);


  char print_buffer[100];

  while(true) {
    if(drive_state_machine.tick()){
      struct drive::PhaseVoltages motor_voltage;
      foc::make_voltage_at_electrical_angle(500, 0, angle, &motor_voltage);
      drive::set_target_phase_voltages(&motor_voltage);

      angle += 8;
    }
    
    // Compute all non-current analog values
    if(analog_update_task.tick()){
      analog::read_bus_voltage_v();
    }


    if(blink_task.tick()){
      // Toggle timer
      led::set_hsv(count / 10.0f, 1.0f, 1.0f);
      count += 1;
    }
  }
}

void tim_elapsed_callback(TIM_HandleTypeDef *htim) {
  if(htim->Instance == TIM6) {
    drive::commutation_interrupt();
  }
}

void gpio_interrupt_callback(uint16_t pin){
  if(pin == IFA_Pin || pin == IFB_Pin){
    encoder::interrupt(pin);
  } else if(pin == IFC_Pin) {
    encoder::index_interrupt();
  }
};

void usb_callback(uint8_t *Buf, uint32_t Len){
  for (int i = 0; i < Len; i++) {
    // Check for buffer overflow
    if (cdcRxBufferIndex < CDC_RX_BUFFER_SIZE) {
        cdcRxBuffer[cdcRxBufferIndex++] = Buf[i];
    }

    if(Buf[i] == '\r'){
      // Handle new command
      comms::slcan_usb_rx(cdcRxBuffer, cdcRxBufferIndex);
      cdcRxBufferIndex = 0;
    }
  }
}