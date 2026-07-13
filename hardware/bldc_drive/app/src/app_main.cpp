#include "app_main.h"

// c lib includes
#include "main.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal_fdcan.h"
#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_flash_ex.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_spi.h"
#include "math.h"
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
#include "uid_hash.h"
#include "comms.h"
#include "drive_memory.h"
#include "foc.h"
// Common lib includes
#include "common/tle5012/tle5012.h"
#include "common/scheduler/scheduler.h"
#include "common/memory/memory.h"
#include "schema/common_messages.h"


void tx_fn(const uint8_t* data, size_t len){
  comms::transmit(data, len);
}
ipc::IPCRouter router(tx_fn);

tle5012b::TLE5012B encoder(&hspi1, ENC_CS_GPIO_Port, ENC_CS_Pin);
DriveMemory drive_memory;

// Request state change
void state_change(StateChange requested_state){
  StateChange return_state;

  return_state.old_state = drive::get_current_state();
  return_state.new_state = drive::request_new_state((drive::DriveState) requested_state.new_state);

  router.send(STATE_CHANGE_ID, StateChange::encode(return_state)); // TODO: Do this automatically via the return
}

// Read out volatile memory
void read_memory(EmptyMessage msg) {
  router.send(READ_MEMORY_ID, DriveMemory::encode(drive_memory));
}

// Write new memory
void write_memory(DriveMemory _new_memory) {
  drive_memory = _new_memory;
  memory::write_memory(&drive_memory);
  router.send(READ_MEMORY_ID, DriveMemory::encode(drive_memory));
}

void set_controller_settings(ControllerSettings controller_settings){
  drive::set_controller_settings(controller_settings);
  router.send(CONTROLLER_SETTINGS_ID, EmptyMessage::encode(EmptyMessage({1})));
}

void set_controller_setpoint(ControllerSetpoint controller_setpoint){
  drive::set_controller_setpoint(controller_setpoint);

  ControllerFeedback feedback;
  feedback.position_rads = encoder.angle_rads;
  feedback.velocity_radsps = encoder.speed_radsps;
  feedback.torque_Nm = drive::get_Q_current();
  feedback.motor_temp_C = analog::get_motor_temp();

  router.send(CONTROLLER_SETPOINT_ID, ControllerFeedback::encode(feedback));
}

// Read raw encoder values
void read_encoder(EmptyMessage msg) {
  EncoderState encoder_state;

  encoder_state.angle_rads = encoder.angle_rads;
  encoder_state.angle_raw = encoder.angle_raw;
  encoder_state.speed_raw = encoder.speed_raw;
  encoder_state.revolutions = encoder.revolutions;

  router.send(ENCODER_STATE_ID, EncoderState::encode(encoder_state));
}

void read_foc_info(EmptyMessage msg) {
  drive::PhaseVoltages phase_voltages = drive::get_target_phase_voltages();
  drive::PhaseCurrents phase_currents = drive::get_phase_currents();
  drive::MotorCurrents motor_currents = drive::get_motor_currents();


  FOCInfo foc_info;
  foc_info.phaseA_V = phase_voltages.phaseA_V;
  foc_info.phaseB_V = phase_voltages.phaseB_V;
  foc_info.phaseC_V = phase_voltages.phaseC_V;

  foc_info.phaseA_mA = phase_currents.phaseA_mA;
  foc_info.phaseB_mA = phase_currents.phaseB_mA;
  foc_info.phaseC_mA = phase_currents.phaseC_mA;

  foc_info.Q_mA = motor_currents.Q_mA;
  foc_info.D_mA = motor_currents.D_mA;

  router.send(7, FOCInfo::encode(foc_info));
}

void set_raw_voltage(RawVoltage msg){
  drive::PhaseVoltages phase_voltages;
  phase_voltages.phaseA_V = msg.phaseA_V;
  phase_voltages.phaseB_V = msg.phaseB_V;
  phase_voltages.phaseC_V = msg.phaseC_V;

  drive::set_target_phase_voltages(&phase_voltages);
}

void app_main(void){
  CDC_RegisterRxCallback(usb_callback);
  HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);

  // Register IPC callbacks 
  router.register_hook<StateChange>(STATE_CHANGE_ID, "state_change", state_change);
  router.register_hook<EmptyMessage>(ENCODER_STATE_ID, "read_encoder", read_encoder);

  router.register_hook<ControllerSettings>(CONTROLLER_SETTINGS_ID, "controller_settings", set_controller_settings);
  router.register_hook<ControllerSetpoint>(CONTROLLER_SETPOINT_ID, "controller_setpoint", set_controller_setpoint);
  router.register_hook<RawVoltage>(RAW_VOLTAGE_ID, "raw_voltage", set_raw_voltage);

  router.register_hook<EmptyMessage>(READ_MEMORY_ID, "read_memory", read_memory);
  router.register_hook<DriveMemory>(WRITE_MEMORY_ID, "write_memroy", write_memory);
  
  // Initialize modules
  memory::initialize(sizeof(DriveMemory), 63);
  drive_memory = *(DriveMemory*)(memory::get_addr()); // Load memory
  if(drive_memory.magic != 0x1234){ // If no memory, load some default values
    drive_memory.can_id = 1; // Default to ID 1
  }
  scheduler::initialize();
  uid_hash::initialize();
  trig::initialize();
  led::initialize();
  comms::initialize(&hfdcan1, &router);
  analog::initialize();
  drive::assign_encoder(&encoder);

  // If voltage is above, init drive, otherwise, boot in error
  if(analog::read_bus_voltage_v() > drive::min_voltage){
    drive::request_new_state(drive::DRIVE_DISABLE);
  } else {
    drive::request_new_state(drive::DRIVE_ERROR);
  }


  // Set up tasks
  scheduler::PeriodicTask blink_task(&htim2);
  blink_task.period_us = scheduler::period_from_hertz(60);

  scheduler::PeriodicTask drive_state_machine(&htim2);
  drive_state_machine.period_us = scheduler::period_from_hertz(100);

  scheduler::PeriodicTask analog_update_task(&htim2);
  analog_update_task.period_us = scheduler::period_from_hertz(50);


  char print_buffer[100];
  float led_timer_s = 0.0f;

  drive::initialize();

  while(true) {
    if(drive_state_machine.tick()){
      drive::state_machine();
    }
    
    // Compute all non-current analog values
    if(analog_update_task.tick()){
      // encoder.update_state();
      analog::read_bus_voltage_v();
    }


    if(blink_task.tick()){

      led_timer_s += blink_task.period_us / 1000000.0f;

      float v_scale = 0.4f;

      switch(drive::get_current_state()) {
        case drive::DriveState::DRIVE_DISABLE:
          led::set_hsv(20.0f, 1.0f, sin(led_timer_s) * v_scale + v_scale + 0.1);
          break;
        case drive::DriveState::DRIVE_CALIBRATE:
          led::set_hsv(290.0f, 1.0f, sin(led_timer_s * 4.0f) * v_scale + v_scale + 0.1);
          break;
        case drive::DriveState::DRIVE_IDLE:
          led::set_hsv(230.0f, 1.0f, sin(led_timer_s * 4.0f) * v_scale + v_scale + 0.1);
          break;
        case drive::DriveState::DRIVE_IMPEDANCE_CONTROL:
          led::set_hsv(led_timer_s * 50.0f, 1.0f, 1.0f);
          break;
        case drive::DriveState::DRIVE_RAW_VOLTAGE_CONTROL:
          led::set_hsv(200.0f, 1.0f,  sin(led_timer_s * 4.0f) * v_scale + v_scale + 0.1);
          break;
        case drive::DriveState::DRIVE_ERROR:
          led::set_hsv(0.0f, 1.0f, sin(led_timer_s * 4.0f) * v_scale + v_scale + 0.1);
          break;
        default:
          led::set_hsv(led_timer_s * 4.0f, 1.0f, 1.0f);
      }

      // Reset after an hour
      if(led_timer_s > 3600.0f){
        led_timer_s = 0;
      }
    }

    // If we have pending can messages
    if(HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0){
      comms::handle_can_rx(&hfdcan1);
    }
  }
}

void tim_elapsed_callback(TIM_HandleTypeDef *htim) {
  if(htim->Instance == TIM6) {
    encoder.update_state(10000);
    analog::update_current_sense();
    drive::commutation_interrupt();
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
      comms::slcan_usb_rx(cdcRxBuffer, cdcRxBufferIndex);
      cdcRxBufferIndex = 0;
    }
  }
}