#pragma once
#include "stm32g4xx_hal.h"
#include "tim.h"

#include "analog.h"

#define PHASE_DUTY_MAX 1024.0f
#define MAX_VOLTAGE 2.0f

namespace drive
{

void initialize();
void enable();
void enable_low_side();
void disable();
void disable_low_side();

void apply_phaseA_duty(uint16_t);
void apply_phaseB_duty(uint16_t);
void apply_phaseC_duty(uint16_t);
void apply_phase_duties(uint16_t, uint16_t, uint16_t);

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

struct PhaseVoltages_mV16 {
    int16_t phaseA_mV;
    int16_t phaseB_mV;
    int16_t phaseC_mV;
};

struct PhaseCurrents {
    float phaseA_mA;
    float phaseB_mA;
    float phaseC_mA;
};

struct PhaseCurrents_mA16 {
    int16_t phaseA_mA;
    int16_t phaseB_mA;
    int16_t phaseC_mA;
};


void set_target_phase_voltages(struct drive::PhaseVoltages* target_voltages);
void apply_phase_voltages(PhaseVoltages*);


void commutation_interrupt();
void state_machine();
void set_gains(float, float);
DriveState request_new_state(DriveState);

} // drive