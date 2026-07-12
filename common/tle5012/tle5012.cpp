#include "tle5012.h"
#include <cstdint>

using namespace tle5012b;

float speed_filter = 0.98;

void tle5012b::TLE5012B::read_register(uint8_t address, uint8_t* _buffer, size_t len){
    // Test SPI
    uint8_t buffer[4];
    uint16_t command = 0;
    uint8_t rw = 1;
    uint8_t lock = 0b000;
    uint8_t upd = 0;
    // uint8_t addr = 2;
    // uint8_t nd = 1;

    command |= rw << 15;
    command |= lock << 11;
    command |= address << 10;
    command |= address << 4;
    command |= len;

    buffer[0] = command >> 8;
    buffer[1] = command & (0xff);

    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi, buffer, 2, 1);
    HAL_SPI_Receive(hspi, _buffer, len * 2, 1); // Ignores safety word for now
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

// TODO: make this update_period_us 
// TODO: speed this up a bit, there's some redundant calculations happening
void tle5012b::TLE5012B::update_state(uint16_t update_period_hz){
    uint8_t data[8];
    read_register(0x02, data, 1);
    uint16_t new_angle_raw = (data[0] << 8 | data[1]) & 0x7FFF;

    // Is all this math better than just doing another 32 byte read?
    int16_t delta = new_angle_raw - angle_raw;
    if(delta < -16384){
        revolutions += 1;
        delta -= 32768;
    } else if(delta > 16384){
        revolutions -= 1;
        delta += 32768;
    }
    speed = speed_filter * speed + (1 - speed_filter) * (delta * update_period_hz);
    speed_raw = (speed);

    angle_raw = new_angle_raw;

    angle_rads = (angle_raw / 32768.0f + revolutions ) * 2.0f * 3.14159274f;
    speed_radsps = (speed / 32768.0f) * 2.0f * 3.14159274f;


    // speed_raw = (((data[2] << 8 | data[3]) & 0x7FFF) ^ 1<<14) - (1<<14);
    // revolutions = (((data[4] << 8 | data[5]) & 0x01FF) ^ 1<<8) - (1<<8);
}
