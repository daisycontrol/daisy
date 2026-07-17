#include "imu.h"
#include "math.h"

using namespace imu;

struct bmi2_dev bmi;
struct bmi2_sens_data rawData;

struct bmm150_dev bmm;
struct bmm150_mag_data mag_data;
struct bmm150_mag_data initial_mag_data;

// Sensor configs, 0 is ACCEL and 1 is GYRO
struct bmi2_sens_config configs[3];

// Current data
struct imu_data latest_data;
bool sensor_running = false;

// Scaling values
double gyro_scale;
double accel_scale;

// Estimates
struct Quaternion heading_estimate;
int prev_sens_time = 0;
#define LOOP_RATE 200.0f;
//TODO: Adjust based on sensor time
float dt_seconds = 1/LOOP_RATE; // 200Hz update rate

void imu::start_bmi270(){
    sensor_running = false;

    // Start timer for microsecond delay
    HAL_TIM_Base_Start(&htim2);

    // Set up hardware shim
    bmi.read = read_i2c;
    bmi.write = write_i2c;
    bmi.delay_us = delay_us;
    bmi.read_write_len = 32;

    bmi.intf = BMI2_I2C_INTF;

    int8_t err = BMI2_OK;

    // Init chip
    err = bmi270_init(&bmi);
    // Read out gyro and accel configs
    configs[ACCEL_CONFIG_INDEX].type = BMI2_ACCEL;
    configs[GYRO_CONFIG_INDEX].type = BMI2_GYRO;
    configs[AUX_CONFIG_INDEX].type = BMI2_AUX;
    err = bmi270_get_sensor_config(configs, 3, &bmi);

    // Set gyro, accel and aux configs
    configs[ACCEL_CONFIG_INDEX].cfg.acc.odr = BMI2_ACC_ODR_400HZ;
    configs[ACCEL_CONFIG_INDEX].cfg.acc.range = BMI2_ACC_RANGE_2G;
    configs[ACCEL_CONFIG_INDEX].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

    configs[GYRO_CONFIG_INDEX].cfg.gyr.odr = BMI2_GYR_ODR_400HZ;
    configs[GYRO_CONFIG_INDEX].cfg.gyr.range = BMI2_GYR_RANGE_1000;
    configs[GYRO_CONFIG_INDEX].cfg.gyr.noise_perf = BMI2_PERF_OPT_MODE;
    configs[GYRO_CONFIG_INDEX].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

    configs[AUX_CONFIG_INDEX].cfg.aux.odr = BMI2_AUX_ODR_400HZ;
    configs[AUX_CONFIG_INDEX].cfg.aux.aux_en = BMI2_ENABLE;
    configs[AUX_CONFIG_INDEX].cfg.aux.i2c_device_addr = BMM150_DEFAULT_I2C_ADDRESS;
    // configs[AUX_CONFIG_INDEX].cfg.aux.i2c_device_addr = 0x5;
    configs[AUX_CONFIG_INDEX].cfg.aux.fcu_write_en = BMI2_ENABLE;
    configs[AUX_CONFIG_INDEX].cfg.aux.manual_en = BMI2_ENABLE;
    configs[AUX_CONFIG_INDEX].cfg.aux.aux_rd_burst = BMI2_AUX_READ_LEN_2;
    configs[AUX_CONFIG_INDEX].cfg.aux.man_rd_burst = BMI2_AUX_READ_LEN_3;
    configs[AUX_CONFIG_INDEX].cfg.aux.read_addr = BMM150_REG_DATA_X_LSB;
    
    err = bmi270_set_sensor_config(configs, 3, &bmi);

    update_scaling_values();

    // Enable i2c pullups for AUX IF
    uint8_t regdata = BMI2_ASDA_PUPSEL_10K;
    err = bmi2_set_regs(BMI2_AUX_IF_TRIM, &regdata, 1, &bmi);


    // Configure BMM150
    bmm.intf_ptr = &bmi;
    bmm.read = aux_read_i2c;
    bmm.write = aux_write_i2c;
    bmm.delay_us = aux_delay_us;
    bmm.intf = BMM150_I2C_INTF;

    // Configure aux sensor
    struct bmm150_settings bmm1_sett;
    bmm1_sett.preset_mode = BMM150_PRESETMODE_LOWPOWER; // not quite sure why
    bmm1_sett.pwr_mode = BMM150_POWERMODE_FORCED;

    err = bmm150_init(&bmm);
    err = bmm150_set_presetmode(&bmm1_sett, &bmm);
    err = bmm150_set_op_mode(&bmm1_sett, &bmm);

    // Disable manual mode on BMM
    configs[AUX_CONFIG_INDEX].cfg.aux.manual_en = BMI2_DISABLE;
    err = bmi270_set_sensor_config(configs, 3, &bmi);

    // Enable sensors
    uint8_t features[] = {BMI2_ACCEL, BMI2_GYRO};

    // BMI2_AUX
    err = bmi270_sensor_enable(features, 2, &bmi);


    // Set up interrupt
    bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, &bmi);

    // Configure interrupt behaviour
    // INT 1 pin, active high (rising-edge) and non-latch
    struct bmi2_int_pin_config int_pin_config;
    int_pin_config.pin_type = BMI2_INT1; 
    int_pin_config.int_latch = BMI2_INT_NON_LATCH;
    int_pin_config.pin_cfg->input_en = BMI2_INT_INPUT_DISABLE;
    int_pin_config.pin_cfg->lvl = BMI2_INT_ACTIVE_HIGH; // Rising edge
    int_pin_config.pin_cfg->od = BMI2_INT_PUSH_PULL;
    int_pin_config.pin_cfg->output_en = BMI2_INT_OUTPUT_ENABLE;
    bmi2_set_int_pin_config(&int_pin_config, &bmi);



    // Set up heading estimator
    heading_estimate.w = 1.0f;
    heading_estimate.x = 0.0f;
    heading_estimate.y = 0.0f;
    heading_estimate.z = 0.0f;

    sensor_running = true;
}

// Interrupt 1 is set up as a data-ready flag
void imu::imu_interrupt_1() {
    update_imu_data();
}

void imu::imu_interrupt_2() {

}

void imu::update_scaling_values() {
    accel_scale = ((2 << configs[0].cfg.acc.range) / 32768.0);
    gyro_scale = ((125 * (1 << (BMI2_GYR_RANGE_125 - configs[1].cfg.gyr.range))) / 32768.0);
}

// https://stanford.edu/class/ee267/lectures/lecture10.pdf
void imu::update_imu_data() {

    if(!sensor_running){
        return;
    }

    // Read sensor data
    int8_t err = BMI2_OK;
    err = bmi2_get_sensor_data(&rawData, &bmi);

    bmm150_aux_mag_data(rawData.aux_data, &mag_data, &bmm);

    // Scale and set latest data
    latest_data.gyro.x = rawData.gyr.x * gyro_scale * DEG_TO_RAD;
    latest_data.gyro.y = rawData.gyr.y * gyro_scale * DEG_TO_RAD;
    latest_data.gyro.z = rawData.gyr.z * gyro_scale * DEG_TO_RAD + 0.00151; //  + 0.0015

    latest_data.accel.x = rawData.acc.x * accel_scale;
    latest_data.accel.y = rawData.acc.y * accel_scale;
    latest_data.accel.z = rawData.acc.z * accel_scale;

    // TODO: Handle overflow !
    // It will run for 49 days supposedly so be careful lol
    // Each tick in sensor time is 39.0625 microseconds for.. whatever reason
    // something doesn't quite work about this....
    // dt_seconds = (rawData.sens_time - prev_sens_time) * 3.90625e-5;
    // prev_sens_time = rawData.sens_time;

    // Wait for data to start outputting
    if(rawData.status == 208 || rawData.status == 240) {
        // return;
    } else {
        return;
    }
        
    // GYRO PREDICTION
    struct Quaternion angular_velocity_local;
    angular_velocity_local.w =  0;
    angular_velocity_local.x = latest_data.gyro.x;
    angular_velocity_local.y = latest_data.gyro.y;
    angular_velocity_local.z = latest_data.gyro.z;

    // Convert angular velocity from local to global frame
    struct Quaternion angular_velocity_global;
    angular_velocity_global = multiply_quaternion( multiply_quaternion(heading_estimate, angular_velocity_local), invert_quaternion(heading_estimate) );

    // Perform quaternion derivative
    struct Quaternion heading_dot;
    heading_dot = scale_quaternion(multiply_quaternion(angular_velocity_global, heading_estimate), 0.5f);

    // Add quaternion (only possible due to quaternion derivative)
    heading_estimate = sum_quaternion(heading_estimate, scale_quaternion(heading_dot, dt_seconds));
    // TODO: Is this needed
    heading_estimate = normalize_quaternion(heading_estimate);

    // Make local gravity quaternion
    struct Quaternion gravity_local_quat;
    gravity_local_quat.w = 0;
    gravity_local_quat.x = latest_data.accel.x;
    gravity_local_quat.y = latest_data.accel.y;
    gravity_local_quat.z = latest_data.accel.z;

    // Calculate global gravity quaternion 
    struct Quaternion estimated_global_gravity_quat;
    estimated_global_gravity_quat = multiply_quaternion( multiply_quaternion(heading_estimate, gravity_local_quat), invert_quaternion(heading_estimate) );

    // Calculate rotation that will move the local estimated gravity towards the known global gravity
    struct Quaternion gravity_error_rotation;
    gravity_error_rotation.w = sqrtf((estimated_global_gravity_quat.z + 1.0f) / 2.0f);
    gravity_error_rotation.x = - (estimated_global_gravity_quat.y) / sqrtf(2.0f * (estimated_global_gravity_quat.z + 1.0f));
    gravity_error_rotation.y = (estimated_global_gravity_quat.x) / sqrtf(2.0f * (estimated_global_gravity_quat.z + 1.0f));
    gravity_error_rotation.z = 0;

    // If gravity is roughly correct, update
    float acc_mag = norm_vector(latest_data.accel);
    if(acc_mag > 0.8 && acc_mag < 1.2 && !isnan(gravity_error_rotation.w)){
        heading_estimate = multiply_quaternion(invert_quaternion(lerp_quaternion_with_identity(gravity_error_rotation, 0.01f)), heading_estimate);
    }

    // Normalize!
    heading_estimate = normalize_quaternion(heading_estimate);
}

struct imu_data get_imu_data() {
    return latest_data;
}



// ======= HARDWARE SHIM

// Read from i2c registers
BMI2_INTF_RETURN_TYPE imu::read_i2c(uint8_t regAddress, uint8_t* dataBuffer, uint32_t numBytes, void* interfacePtr)
{
    HAL_StatusTypeDef return_code = HAL_I2C_Master_Transmit(&hi2c1, BMI270_ADDRESS, &regAddress, 1, HAL_MAX_DELAY);
    return_code = HAL_I2C_Master_Receive(&hi2c1, BMI270_ADDRESS, dataBuffer, numBytes, HAL_MAX_DELAY);
    
    return BMI2_OK;
}

BMI2_INTF_RETURN_TYPE imu::aux_read_i2c(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    int8_t return_code;
    return_code = bmi2_read_aux_man_mode(reg_addr, reg_data, (uint16_t)length, &bmi);
    return return_code;
}

// Write to i2c registers
BMI2_INTF_RETURN_TYPE imu::write_i2c(uint8_t regAddress, const uint8_t* dataBuffer, uint32_t numBytes, void* interfacePtr)
{
    // HAL_StatusTypeDef return_code = HAL_I2C_Master_Transmit(&hi2c1, BMI270_ADDRESS, &regAddress, 1, HAL_MAX_DELAY);
    // return_code = HAL_I2C_Master_Transmit(&hi2c1, BMI270_ADDRESS, &dataBuffer, numBytes, HAL_MAX_DELAY);

    HAL_StatusTypeDef return_code = HAL_I2C_Mem_Write(&hi2c1, BMI270_ADDRESS, regAddress, I2C_MEMADD_SIZE_8BIT, const_cast<uint8_t*>(dataBuffer), numBytes, HAL_MAX_DELAY);
    return BMI2_OK;
}

BMI2_INTF_RETURN_TYPE imu::aux_write_i2c(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    int8_t return_code;
    return_code = bmi2_write_aux_man_mode(reg_addr, reg_data, (uint16_t)length, &bmi);
    return return_code;
}

// Delay microseconds
// Credit: https://controllerstech.com/create-1-microsecond-delay-stm32/
void imu::delay_us(uint32_t period, void* interfacePtr)
{
    __HAL_TIM_SET_COUNTER(&htim2,0);  // set the counter value a 0
	while (__HAL_TIM_GET_COUNTER(&htim2) < period);  // wait for the counter to reach the us input in the parameter
}

void imu::aux_delay_us(uint32_t period, void* interfacePtr){
    delay_us(period, interfacePtr);
}