#include "analog.h"
#include "math.h"

using namespace analog;

uint16_t adc1_dma[5];
uint16_t adc2_dma[3];
uint16_t adc2_dma_offset_measure[3];
uint16_t adc2_dma_offset[3];
float current_sense[3];
float cached_bus_voltage_v;

bool zero_current_sense = false;

void analog::initialize(){
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&hadc1, reinterpret_cast<uint32_t*>(adc1_dma), 5);

    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADC_Start_DMA(&hadc2, reinterpret_cast<uint32_t*>(adc2_dma), 3);

    analog::calibrate_current_sense();
}

float analog::read_bus_voltage_v(){
    cached_bus_voltage_v = adc1_dma[0] * (0.000806f / 0.0637f) * 1.006f;
    return cached_bus_voltage_v;
}

float analog::get_cached_bus_voltage_v(){
    return cached_bus_voltage_v;
}

void analog::calibrate_current_sense(){
    // Perform DRV amp calibration
    HAL_GPIO_WritePin(DRV_CAL_GPIO_Port, DRV_CAL_Pin, (GPIO_PinState)(0));
    HAL_Delay(2);
    HAL_GPIO_WritePin(DRV_CAL_GPIO_Port, DRV_CAL_Pin, (GPIO_PinState)(1));
    // Wait for internal calibration
    HAL_Delay(20);

    // Zero out offsets
    adc2_dma_offset[0] = 0;
    adc2_dma_offset[1] = 0;
    adc2_dma_offset_measure[0] = 2048;
    adc2_dma_offset_measure[1] = 2048;
    // Measure new offset
    zero_current_sense = true;
    HAL_Delay(10);

    // Apply new offset and disable drv calibration
    zero_current_sense = false;
    adc2_dma_offset[0] = adc2_dma_offset_measure[0]; // unsure why these constant offsets are needed,.
    adc2_dma_offset[1] = adc2_dma_offset_measure[1];


    HAL_GPIO_WritePin(DRV_CAL_GPIO_Port, DRV_CAL_Pin, (GPIO_PinState)(0));
}

void analog::update_current_sense(){
    // Run IIR filter on current
    // TODO: Make this less of a mess
    current_sense[0] = current_sense[0] * CURRENT_FILTER_CONSTANT + ((adc2_dma[0] - adc2_dma_offset[0])) * (1.0f - CURRENT_FILTER_CONSTANT);
    current_sense[1] = current_sense[1] * CURRENT_FILTER_CONSTANT + ((adc2_dma[1] - adc2_dma_offset[1])) * (1.0f - CURRENT_FILTER_CONSTANT);
    current_sense[2] = - current_sense[0] - current_sense[1];

    if(zero_current_sense){
        adc2_dma_offset_measure[0] = adc2_dma_offset_measure[0] * ZERO_FILTER_CONSTANT + ((adc2_dma[0])) * (1.0f - ZERO_FILTER_CONSTANT);
        adc2_dma_offset_measure[1] = adc2_dma_offset_measure[1] * ZERO_FILTER_CONSTANT + ((adc2_dma[1])) * (1.0f - ZERO_FILTER_CONSTANT);
    }
}

// TODO: Speed this up? Feels a bit bloated
#define BETA_FET 4050.0f
#define BETA_MOT 4000.0f 
#define ROOM_TEMP 298.15f
#define ADC_MAX 4096.0f
#define R_1 10.0f
#define FET_R_ROOM_TEMP 47.0f
#define MOT_R_ROOM_TEMP 10.0f
#define KELVIN_TO_C 273.15f

#define ADC_ADJUSTMENT_FACTOR_FET 150
#define ADC_ADJUSTMENT_FACTOR_MOT 100

float analog::get_fet_temp(){
    // Calculate ntc resistance given the other parameters in the voltage divider
    float r_ntc = R_1 / ((ADC_MAX / ((float) adc1_dma[4] + ADC_ADJUSTMENT_FACTOR_FET)) - 1);
    
    float fet_temp_kelvin = (BETA_FET * ROOM_TEMP) /
                            (BETA_FET + (ROOM_TEMP * logf(r_ntc / FET_R_ROOM_TEMP)));

    // Convert from kelvin to C
    volatile float fet_temp_C = fet_temp_kelvin - KELVIN_TO_C;
    
    return fet_temp_C;
}

float analog::get_motor_temp(){
    // Calculate ntc resistance given the other parameters in the voltage divider
    float r_ntc = R_1 / ((ADC_MAX / ((float) adc1_dma[3] + ADC_ADJUSTMENT_FACTOR_MOT)) - 1);
    
    float mot_temp_kelvin = (BETA_MOT * ROOM_TEMP) /
                            (BETA_MOT + (ROOM_TEMP * logf(r_ntc / MOT_R_ROOM_TEMP)));

    // Convert from kelvin to c
    volatile float mot_temp_C = mot_temp_kelvin - KELVIN_TO_C;
    
    return mot_temp_C;
}

float analog::get_phase_A_current_mA(){
    return current_sense[0] * 9;
}

float analog::get_phase_B_current_mA(){
    return current_sense[1] * 9;
}

float analog::get_phase_C_current_mA(){
    return current_sense[2] * 9;
}
