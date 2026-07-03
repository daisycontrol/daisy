#include "app_main.h"
// c lib includes
#include "main.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_flash_ex.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_spi.h"
#include "string.h"
#include <stdio.h>
// ST HAL includes
#include "stm32g4xx_hal.h"
#include "tim.h"
#include "fdcan.h"
#include "usbd_cdc_if.h"
#include "spi.h"
// Custom lib includes
#include "structs.h"
#include "analog.h"
#include "drive.h"
#include "led.h"
#include "trig.h"
#include "foc.h"
#include "uid_hash.h"
#include "comms.h"
// Common lib includes
#include "common/tle5012/tle5012.h"
#include "common/scheduler/scheduler.h"
#include "common/memory/memory.h"
#include "schema/common_messages.h"
#include "hardware/bldc_drive/bldc_drive_messages.h"


// struct ControllerTarget target;
int count = 0;
int angle = 0;

#define CDC_RX_BUFFER_SIZE 128
uint8_t cdcRxBuffer[CDC_RX_BUFFER_SIZE];
uint32_t cdcRxBufferIndex = 0;

void tx_fn(const uint8_t* data, size_t len){
  comms::transmit(data, len);
}

ipc::IPCRouter router(tx_fn);
tle5012b::TLE5012B encoder(&hspi1, ENC_CS_GPIO_Port, ENC_CS_Pin);


#define STATE_CHANGE_ID 1
#define ENCODER_STATE_ID 5

void state_change(StateChange requested_state){
  StateChange return_state;

  return_state.old_state = drive::get_current_state();
  return_state.new_state = drive::request_new_state((drive::DriveState) requested_state.new_state);

  router.send(STATE_CHANGE_ID, StateChange::encode(return_state)); // TODO: Do this automatically via the return
}


DriveMemory drive_memory;

// #define SETTINGS_BANK FLASH_BANK_1
// #define SETTINGS_PAGE (63)
// #define SETTINGS_PAGE_ADDR (0x08000000UL + (SETTINGS_PAGE * FLASH_PAGE_SIZE))
// #define SETTINGS_PAGE_SIZE FLASH_PAGE_SIZE
// #define SETTINGS_RECORD_SIZE_ALIGNED ((sizeof(DriveMemory) + 7u) & ~7u)


void read_encoder(EmptyMessage msg){
  EncoderState encoder_state;

  encoder_state.angle_raw = encoder.angle_raw;
  encoder_state.speed_raw = encoder.speed_raw;
  encoder_state.revolutions = encoder.revolutions;

  router.send(ENCODER_STATE_ID, EncoderState::encode(encoder_state));
}

void app_main(void){
  CDC_RegisterRxCallback(usb_callback);

  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);

  ControllerSetpoint setpoint;
  setpoint.position_target_mRads = 10;

  router.register_hook<StateChange>(STATE_CHANGE_ID, "state_change", state_change);
  // router.register_hook<StateChange>(1, "state_change", test_fn);
  router.register_hook<EmptyMessage>(ENCODER_STATE_ID, "read_encoder", read_encoder);

  // Attempt to load memory
  // memory = *(DriveMemory *)(SETTINGS_PAGE_ADDR);

  drive_memory.magic = 0x12345678;
  



  // Some modules need to be re-inited based on drive voltage state
  memory::initialize(sizeof(DriveMemory), 63);
  memory::write_memory(&drive_memory);


  scheduler::initialize();
  uid_hash::initialize();
  trig::initialize();
  drive::initialize();
  analog::initialize();
  led::initialize();
  comms::initialize(&hfdcan1, &router);

  scheduler::PeriodicTask blink_task(&htim2);
  blink_task.period_us = scheduler::period_from_hertz(60);

  scheduler::PeriodicTask drive_state_machine(&htim2);
  drive_state_machine.period_us = scheduler::period_from_hertz(1000);

  scheduler::PeriodicTask analog_update_task(&htim2);
  analog_update_task.period_us = scheduler::period_from_hertz(100);

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
      encoder.update_state();
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