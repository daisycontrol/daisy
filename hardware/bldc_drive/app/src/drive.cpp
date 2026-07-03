#include "drive.h"
#include "analog.h"
#include "common/tle5012/tle5012.h"
#include "foc.h"
#include "utils.h"
#include "stm32g4xx_hal.h"
#include "tim.h"
#include "analog.h"

using namespace drive;

struct PhaseVoltages phase_voltages;

enum DriveState drive_state = DriveState::DRIVE_DISABLE;
enum DriveError drive_error = DriveError::ERROR_NONE;
enum InitState init_state = InitState::INIT_INIT;
enum CalibrationState calibration_state = CalibrationState::CALIB_ROTATE_FORWARD;

float kP_Nm;
float kD_Nm;

float const drive::min_voltage = 12.0f;

tle5012b::TLE5012B* drive_encoder;

void drive::assign_encoder(tle5012b::TLE5012B* _encoder){
    drive_encoder = _encoder;
}

void drive::initialize(){
    analog::initialize();

    drive::enable();
    drive::enable_low_side();

    // Start phase PWM timers
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    // 10kHz commutation interrupt
    HAL_TIM_Base_Start_IT(&htim6);
}

void drive::enable(){
    HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, (GPIO_PinState) 1);
}

void drive::enable_low_side(){
    HAL_GPIO_WritePin(INLX_GPIO_Port, INLX_Pin, (GPIO_PinState) 1);
}

void drive::disable(){
    HAL_GPIO_WritePin(DRV_EN_GPIO_Port, DRV_EN_Pin, (GPIO_PinState) 0);
}

void drive::disable_low_side(){
    HAL_GPIO_WritePin(INLX_GPIO_Port, INLX_Pin, (GPIO_PinState) 0);
}

void drive::apply_phaseA_duty(uint16_t value){
    TIM1->CCR3 = value;
}

void drive::apply_phaseB_duty(uint16_t value){
    TIM1->CCR2 = value;
}

void drive::apply_phaseC_duty(uint16_t value){
    TIM1->CCR1 = value;
}

void drive::set_target_phase_voltages(struct drive::PhaseVoltages* target_voltages){
    phase_voltages = *target_voltages;
}

void drive::apply_phase_duties(uint16_t value_A, uint16_t value_B, uint16_t value_C){
    //TODO: clip voltages here
    TIM1->CCR3 = value_A;
    TIM1->CCR2 = value_B;
    TIM1->CCR1 = value_C;
}

void drive::apply_phase_voltages(struct drive::PhaseVoltages* voltage){
    float voltage_to_duty = PHASE_DUTY_MAX / analog::get_cached_bus_voltage_v();

    apply_phase_duties(
        PHASE_DUTY_MAX / 2.0f + utils::fbound_sym(voltage->phaseA_V, MAX_VOLTAGE) * voltage_to_duty,
        PHASE_DUTY_MAX / 2.0f + utils::fbound_sym(voltage->phaseB_V, MAX_VOLTAGE) * voltage_to_duty,
        PHASE_DUTY_MAX / 2.0f + utils::fbound_sym(voltage->phaseC_V, MAX_VOLTAGE) * voltage_to_duty
    );
}

void drive::commutation_interrupt(){

    if(drive_state == DriveState::DRIVE_IDLE || drive_state == DriveState::DRIVE_IMPEDANCE_CONTROL){

        // FOC

        drive::apply_phase_voltages(&phase_voltages);
    } else if (drive_state == DriveState::DRIVE_CALIBRATE) {
        drive::apply_phase_voltages(&phase_voltages);
    } else {
        // Neutral PWM
        drive::apply_phase_duties(512, 512, 512);
    }
}

void drive::set_gains(float new_kP_Nm, float new_kD_Nm) {
    kP_Nm = utils::fbound(new_kP_Nm, 0.0, 10.0);
    kD_Nm = utils::fbound(new_kD_Nm, 0.0, 1.0);
}

DriveState drive::request_new_state(DriveState new_state){
    if(new_state == DriveState::DRIVE_DISABLE) {
        drive_state = DriveState::DRIVE_DISABLE;
    }

    if(new_state == DriveState::DRIVE_INIT && 
        (drive_state == DriveState::DRIVE_DISABLE || drive_state == DriveState::DRIVE_ERROR)) {
        drive_error = DriveError::ERROR_NONE;

        // Re-init for good measure
        drive::initialize();
        drive_state = DriveState::DRIVE_INIT;
    }

    if(new_state == DriveState::DRIVE_CALIBRATE && 
        (drive_state == DriveState::DRIVE_DISABLE || drive_state == DriveState::DRIVE_IDLE)) {
        calibration_state = CalibrationState::CALIB_ROTATE_FORWARD;
        drive_state = DriveState::DRIVE_CALIBRATE;
    }

    if(new_state == DriveState::DRIVE_IDLE && 
        (drive_state == DriveState::DRIVE_DISABLE || 
            drive_state == DriveState::DRIVE_IMPEDANCE_CONTROL || 
            drive_state == DriveState::DRIVE_INIT || 
            drive_state == DriveState::DRIVE_CALIBRATE)) {
        drive_state = DriveState::DRIVE_IDLE;
    }

    if(new_state == DriveState::DRIVE_IMPEDANCE_CONTROL && drive_state == DriveState::DRIVE_IDLE) {
        drive_state = DriveState::DRIVE_IMPEDANCE_CONTROL;
    }

    if(new_state == DriveState::DRIVE_ERROR) {
        drive_state = DriveState::DRIVE_ERROR;
    }

    return drive_state;
}

float calibration_target_angle = 0;
const float calibration_speed = 1.0f;
const int calibration_electrical_rotations = 2;
const int calibration_voltage_mV = 500;
uint16_t measured_electrical_angle = 0;
int16_t electrical_angle_offset = 0;

void calibrate_state(){
    switch (calibration_state) {
    case CalibrationState::CALIB_ROTATE_FORWARD:
        calibration_target_angle += calibration_speed;

        foc::make_voltage_at_electrical_angle(0.0f, calibration_voltage_mV, calibration_target_angle, &phase_voltages);
        drive::set_target_phase_voltages(&phase_voltages);
        measured_electrical_angle = foc::convert_to_electrical_angle(drive_encoder->angle_raw, drive_memory.drive_pole_pairs, 0);

        electrical_angle_offset = calibration_target_angle - measured_electrical_angle;

        if(calibration_target_angle > 512 * calibration_electrical_rotations) {
            phase_voltages.phaseA_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseB_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseC_V = analog::get_cached_bus_voltage_v() / 2.0f;

            calibration_state = CalibrationState::CALIB_ROTATE_BACKWARD;
        }
    break;
    case CalibrationState::CALIB_ROTATE_BACKWARD:
        calibration_target_angle -= calibration_speed;

        foc::make_voltage_at_electrical_angle(0.0f, calibration_voltage_mV, calibration_target_angle, &phase_voltages);
        drive::set_target_phase_voltages(&phase_voltages);
        
        if(calibration_target_angle < 0) {
            phase_voltages.phaseA_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseB_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseC_V = analog::get_cached_bus_voltage_v() / 2.0f;

            calibration_state = CalibrationState::CALIB_CALCULATE;
        }
    break;
    case CalibrationState::CALIB_CALCULATE:

        calibration_state = CALIB_DONE;
    break;
    case CalibrationState::CALIB_DONE:
        drive::request_new_state(DriveState::DRIVE_IDLE);
    break;
    }
}

// Put in drive interrupt
void safety_checks(){

}

void drive::state_machine(){
    switch(drive_state){
        case DriveState::DRIVE_DISABLE:

            // If bus voltage is above, init again.
            break;
        case DriveState::DRIVE_INIT:
            // TODO: This hangs for ~10ms due to some HAL_Delays
            drive::initialize();
            request_new_state(DriveState::DRIVE_IDLE);
            break;
        case DriveState::DRIVE_CALIBRATE:
            calibrate_state();
            break;
        case DriveState::DRIVE_IDLE:
            break;
        case DriveState::DRIVE_IMPEDANCE_CONTROL:
            break;
        case DriveState::DRIVE_ERROR:
            // Zero phases
            phase_voltages.phaseA_V = analog::get_cached_bus_voltage_v() / 2.0;
            phase_voltages.phaseB_V = analog::get_cached_bus_voltage_v() / 2.0;
            phase_voltages.phaseC_V = analog::get_cached_bus_voltage_v() / 2.0;

            // Disable DRV
            drive::disable();

            // TODO: Track error type
            if(analog::get_cached_bus_voltage_v() > drive::min_voltage){
                request_new_state(DriveState::DRIVE_IDLE);
            }
            break;
    }
}

DriveState drive::get_current_state(){
    return drive_state;
}