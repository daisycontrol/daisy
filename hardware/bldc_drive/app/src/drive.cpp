#include "drive.h"
#include "analog.h"
#include "common/tle5012/tle5012.h"
#include "drive_memory.h"
#include "foc.h"
#include "utils.h"
#include "stm32g4xx_hal.h"
#include "tim.h"
#include "analog.h"
#include "schema/common_messages.h"

using namespace drive;


struct PhaseVoltages phase_voltages;
struct MotorVoltages motor_voltages;

struct PhaseCurrents phase_curents;
struct MotorCurrents motor_currents;
struct MotorCurrents target_motor_currents;

struct ControllerSettings settings;
struct ControllerSetpoint setpoint;
struct ControllerSetpoint setpoint_filtered;

const float current_kP = 0.5f;
const float current_kI = 0.01f;
float Q_accumulator = 0.0f;
float D_accumulator = 0.0f;

int current_Q_mA;
int current_D_mA;

enum DriveState drive_state = DriveState::DRIVE_DISABLE;
enum DriveError drive_error = DriveError::ERROR_NONE;
enum InitState init_state = InitState::INIT_INIT;
enum CalibrationState calibration_state = CalibrationState::CALIB_ROTATE_FORWARD;

float kP_Nm;
float kD_Nm;

bool started = false;

#define SETPOINT_TIMEOUT_USEC 500000
volatile uint32_t last_setpoint_usec = 0;

float const drive::min_voltage = 12.0f;

tle5012b::TLE5012B* drive_encoder;

void drive::assign_encoder(tle5012b::TLE5012B* _encoder){
    drive_encoder = _encoder;
}

void drive::initialize(){
    drive::enable();

    analog::initialize();

    // Start phase PWM timers
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

    // 10kHz commutation interrupt
    HAL_TIM_Base_Start_IT(&htim6);

    drive::enable_low_side();
    HAL_Delay(15);
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

uint16_t electrical_angle;

void drive::commutation_interrupt(){

    if(drive_state == DriveState::DRIVE_IDLE || drive_state == DriveState::DRIVE_IMPEDANCE_CONTROL){

        // Get electrical angle
        electrical_angle = foc::convert_to_electrical_angle(drive_encoder->angle_raw, drive_memory.drive_pole_pairs, drive_memory.drive_encoder_offset);

        // Find phase currents
        phase_curents.phaseA_mA = analog::get_phase_A_current_mA();
        phase_curents.phaseB_mA = analog::get_phase_B_current_mA();
        phase_curents.phaseC_mA = analog::get_phase_C_current_mA();
        foc::phase_currents_to_motor(&phase_curents, &current_D_mA, &current_Q_mA, electrical_angle);
        motor_currents.D_mA = (float)current_D_mA;
        motor_currents.Q_mA = (float)current_Q_mA;

        if(drive_state == DriveState::DRIVE_IDLE){
            target_motor_currents.Q_mA = 0;
            target_motor_currents.D_mA = 0;
        } else {
            // Filter setpoint
            setpoint_filtered.position_target_rads = settings.position_filter * setpoint_filtered.position_target_rads + (1 - settings.position_filter) * setpoint.position_target_rads;

            // Apply
            target_motor_currents.Q_mA = utils::fbound_sym((setpoint_filtered.position_target_rads - drive_encoder->angle_rads) * settings.kP_Nm 
            + (setpoint.velocity_target_radsps - drive_encoder->speed_radsps) * settings.kD_Nm + setpoint.torque_feedforward_Nm * 1000.0f, settings.max_current_mA);

            target_motor_currents.D_mA = 0;
        }

        float D_error = target_motor_currents.D_mA - current_D_mA;
        float Q_error = target_motor_currents.Q_mA - current_Q_mA;

        D_accumulator += D_error;
        // D_accumulator = utils::fbound_sym(D_accumulator, 100.0f);

        Q_accumulator += Q_error;
        // Q_accumulator = utils::fbound_sym(Q_accumulator, 100.0f);

        motor_voltages.D_V = current_kP * D_error + current_kI * D_accumulator;
        motor_voltages.Q_V = current_kP * Q_error + current_kI * Q_accumulator;
        
        // Apply new phase voltages
        foc::make_voltage_at_electrical_angle(motor_voltages.D_V, motor_voltages.Q_V, electrical_angle, &phase_voltages);        
        drive::apply_phase_voltages(&phase_voltages);
    } else if (drive_state == DriveState::DRIVE_CALIBRATE) {
        drive::apply_phase_voltages(&phase_voltages);
    } else {
        // Neutral PWM
        drive::apply_phase_duties(0, 0, 0);
    }
}

DriveState drive::request_new_state(DriveState new_state){
    if(new_state == DriveState::DRIVE_DISABLE) {
        drive_state = DriveState::DRIVE_DISABLE;
    }

    if(new_state == DriveState::DRIVE_INIT && 
        (drive_state == DriveState::DRIVE_DISABLE || drive_state == DriveState::DRIVE_ERROR)) {
        drive_error = DriveError::ERROR_NONE;
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
            
        // Ensure settings have been written
        if(drive_memory.magic == 0x1234) {
            D_accumulator = 0.0f;
            Q_accumulator = 0.0f;
            drive_state = DriveState::DRIVE_IDLE;
        }
    }

    if(new_state == DriveState::DRIVE_IMPEDANCE_CONTROL && drive_state == DriveState::DRIVE_IDLE) {
         // Ensure settings have been written
        if(drive_memory.magic == 0x1234) {
            setpoint_filtered.position_target_rads = drive_encoder->angle_rads; // Warm start position filter
            last_setpoint_usec = htim2.Instance->CNT; // Reset watchdog
            drive_state = DriveState::DRIVE_IMPEDANCE_CONTROL;
        }
    }

    if(new_state == DriveState::DRIVE_ERROR) {
        drive_state = DriveState::DRIVE_ERROR;
    }

    return drive_state;
}

float calibration_target_angle = 0;
const float calibration_speed = 1.0f;
const int calibration_electrical_rotations = 4;
const int calibration_voltage_mV = 750;
uint16_t measured_electrical_angle = 0;
int average_electrical_angle_offset = 0;

#define ANGLE_COUNTS 1024

void calibrate_state(){
    int offset;
    switch (calibration_state) {
    case CalibrationState::CALIB_ROTATE_FORWARD:
        calibration_target_angle += calibration_speed;

        foc::make_voltage_at_electrical_angle(calibration_voltage_mV, 0.0f, calibration_target_angle, &phase_voltages);
        drive::set_target_phase_voltages(&phase_voltages);
        measured_electrical_angle = foc::convert_to_electrical_angle(drive_encoder->angle_raw, drive_memory.drive_pole_pairs, 0);

        
        offset = measured_electrical_angle - ((int)calibration_target_angle) % ANGLE_COUNTS;
        // Wrap
        if(offset > ANGLE_COUNTS/2){
            offset = offset - ANGLE_COUNTS;
        } else if(offset < - ANGLE_COUNTS/2){
            offset = offset + ANGLE_COUNTS;
        }
        average_electrical_angle_offset += offset;


        if(calibration_target_angle > ANGLE_COUNTS * calibration_electrical_rotations) {
            phase_voltages.phaseA_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseB_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseC_V = analog::get_cached_bus_voltage_v() / 2.0f;

            calibration_state = CalibrationState::CALIB_ROTATE_BACKWARD;
        }
    break;
    case CalibrationState::CALIB_ROTATE_BACKWARD:
        calibration_target_angle -= calibration_speed;

        foc::make_voltage_at_electrical_angle(calibration_voltage_mV, 0.0f, calibration_target_angle, &phase_voltages);
        drive::set_target_phase_voltages(&phase_voltages);
        measured_electrical_angle = foc::convert_to_electrical_angle(drive_encoder->angle_raw, drive_memory.drive_pole_pairs, 0);
        
        offset = measured_electrical_angle - ((int)calibration_target_angle) % ANGLE_COUNTS;
        // Wrap
        if(offset > ANGLE_COUNTS/2){
            offset = offset - ANGLE_COUNTS;
        } else if(offset < - ANGLE_COUNTS/2){
            offset = offset + ANGLE_COUNTS;
        }
        average_electrical_angle_offset += offset;
        
        if(calibration_target_angle < 0) {
            phase_voltages.phaseA_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseB_V = analog::get_cached_bus_voltage_v() / 2.0f;
            phase_voltages.phaseC_V = analog::get_cached_bus_voltage_v() / 2.0f;

            calibration_state = CalibrationState::CALIB_CALCULATE;
        }
    break;
    case CalibrationState::CALIB_CALCULATE:
        drive_memory.drive_encoder_offset = average_electrical_angle_offset / (calibration_electrical_rotations * 2 * 1024);
        average_electrical_angle_offset = 0;
        calibration_state = CALIB_DONE;
    break;
    case CalibrationState::CALIB_DONE:
        drive::request_new_state(DriveState::DRIVE_IDLE);
    break;
    }
}

struct drive::PhaseVoltages drive::get_target_phase_voltages() {
    return phase_voltages;
};

struct drive::PhaseCurrents drive::get_phase_currents() {
    return phase_curents;
};

struct drive::MotorVoltages drive::get_target_motor_voltages() {
    return motor_voltages;
};

struct drive::MotorCurrents drive::get_motor_currents() {
    return motor_currents;
};

float drive::get_Q_current(){
    return motor_currents.Q_mA;
}

// Put in drive interrupt
void limit_checks(){
    if(analog::get_cached_bus_voltage_v() < drive::min_voltage){
        request_new_state(DriveState::DRIVE_ERROR);
    }
}

void drive::state_machine(){
    switch(drive_state){
        case DriveState::DRIVE_DISABLE:
            limit_checks();
            break;
        case DriveState::DRIVE_INIT:
            limit_checks();
            // TODO: This hangs for ~10ms due to some HAL_Delays
            drive::initialize();
            request_new_state(DriveState::DRIVE_IDLE);
            break;
        case DriveState::DRIVE_CALIBRATE:
            limit_checks();
            calibrate_state();
            break;
        case DriveState::DRIVE_IDLE:
            limit_checks();
            break;
        case DriveState::DRIVE_IMPEDANCE_CONTROL:
            limit_checks();
            // if(htim2.Instance->CNT - last_setpoint_usec > SETPOINT_TIMEOUT_USEC){
            //     request_new_state(DRIVE_IDLE); // Is this what I want?? could go to disable or a 
            // }
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
                request_new_state(DriveState::DRIVE_DISABLE);
            }
            break;
    }
}

DriveState drive::get_current_state(){
    return drive_state;
}

void drive::set_controller_settings(ControllerSettings _new_settings){
    settings = _new_settings;
}

void drive::set_controller_setpoint(ControllerSetpoint _new_setpoint){
    // TODO: Handle wraparound
    setpoint = _new_setpoint;
    last_setpoint_usec = htim2.Instance->CNT;
}