#include "tle5012.h"
#include <cstdint>

using namespace tle5012b;

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
    HAL_SPI_Transmit(hspi, buffer, 2, 100);
    HAL_SPI_Receive(hspi, _buffer, len * 2 + 2, 100); // Ignores safety word for now
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

void tle5012b::TLE5012B::update_state(){
    uint8_t data[8];
    read_register(0x02, data, 3);

    angle_raw = (data[0] << 8 | data[1]) & 0x7FFF;
    speed_raw = (((data[2] << 8 | data[3]) & 0x7FFF) ^ 1<<14) - (1<<14);
    revolutions = (((data[4] << 8 | data[5]) & 0x01FF) ^ 1<<8) - (1<<8);
}
