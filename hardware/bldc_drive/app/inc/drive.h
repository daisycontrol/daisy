#pragma once
#include "common/tle5012/tle5012.h"
#include "drive_memory.h"

#define PHASE_DUTY_MAX 1024.0f
#define MAX_VOLTAGE 6.0f

namespace drive
{

void initialize();
void assign_drive_memory(struct DriveMemory* drive_memory);
void assign_encoder(tle5012b::TLE5012B*);
void enable();
void enable_low_side();
void disable();
void disable_low_side();

void apply_phaseA_duty(uint16_t);
void apply_phaseB_duty(uint16_t);
void apply_phaseC_duty(uint16_t);
void apply_phase_duties(uint16_t, uint16_t, uint16_t);

extern const float min_voltage;

enum InitState {
    INIT_INIT,
    INIT_DONE
};

enum CalibrationState {
    CALIB_ROTATE_FORWARD,
    CALIB_ROTATE_BACKWARD,
    CALIB_CALCULATE,
    CALIB_DONE
};

enum DriveState {
    DRIVE_DISABLE,
    DRIVE_INIT,
    DRIVE_CALIBRATE,
    DRIVE_IDLE,
    DRIVE_IMPEDANCE_CONTROL,
    DRIVE_ERROR
};

enum DriveError {
    ERROR_NONE,
    ERROR_FW_ERROR,
    ERROR_NO_SUPPLY,
};

struct PhaseVoltages {
    float phaseA_V;
    float phaseB_V;
    float phaseC_V;
};

struct MotorVoltages {
    float Q_V;
    float D_V;
};

// struct PhaseVoltages_mV16 {
//     int16_t phaseA_mV;
//     int16_t phaseB_mV;
//     int16_t phaseC_mV;
// };

struct PhaseCurrents {
    float phaseA_mA;
    float phaseB_mA;
    float phaseC_mA;
};

struct MotorCurrents {
    float Q_mA;
    float D_mA;
};

// struct PhaseCurrents_mA16 {
//     int16_t phaseA_mA;
//     int16_t phaseB_mA;
//     int16_t phaseC_mA;
// };


void set_target_phase_voltages(struct drive::PhaseVoltages* target_voltages);
struct drive::PhaseVoltages get_target_phase_voltages();
struct drive::PhaseCurrents get_phase_currents();
struct drive::MotorVoltages get_target_motor_voltages();
struct drive::MotorCurrents get_motor_currents();
float get_Q_current();

void apply_phase_voltages(PhaseVoltages*);

void commutation_interrupt();
void state_machine();
void set_gains(float, float);
DriveState request_new_state(DriveState);
DriveState get_current_state();
void set_controller_settings(ControllerSettings);
void set_controller_setpoint(ControllerSetpoint);

} // drive