#pragma once

#include "main.h"
#include "common/transform/quaternion.h"
#include "common/transform/vector.h"
#include "common/bmi270/bmi270.h"
#include "common/bmm150/bmm150.h"
#include "stdbool.h"

#define ACCEL_CONFIG_INDEX 0
#define GYRO_CONFIG_INDEX 1
#define AUX_CONFIG_INDEX 2

#define DEG_TO_RAD 0.0174533


extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;

namespace imu
{

#define BMI270_ADDRESS 0x69 << 1

struct imu_data {
    struct Vector accel;
    struct Vector gyro;
};

void start_bmi270();

void imu_interrupt_1();
void imu_interrupt_2();

void update_scaling_values();

void update_imu_data();
struct imu_data get_imu_data();

// Hardware shim
BMI2_INTF_RETURN_TYPE read_i2c(uint8_t regAddress, uint8_t* dataBuffer, uint32_t numBytes, void* interfacePtr);
BMI2_INTF_RETURN_TYPE write_i2c(uint8_t regAddress, const uint8_t* dataBuffer, uint32_t numBytes, void* interfacePtr);
void delay_us(uint32_t period, void* interfacePtr);

BMI2_INTF_RETURN_TYPE aux_read_i2c(uint8_t regAddress, uint8_t* dataBuffer, uint32_t numBytes, void* interfacePtr);
BMI2_INTF_RETURN_TYPE aux_write_i2c(uint8_t regAddress, const uint8_t* dataBuffer, uint32_t numBytes, void* interfacePtr);
void aux_delay_us(uint32_t period, void* interfacePtr);


}