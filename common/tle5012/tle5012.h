#pragma once
#include <cstdint>
#include "spi.h"

namespace tle5012b
{
    class TLE5012B {
        public:
            SPI_HandleTypeDef* hspi;

            GPIO_TypeDef* cs_port;
            uint16_t cs_pin;

            uint16_t angle_raw;
            int16_t speed_raw;
            int16_t revolutions;
            

            TLE5012B(SPI_HandleTypeDef* _hspi, GPIO_TypeDef* _cs_port, uint16_t _cs_pin) {
                hspi = _hspi;
                cs_port = _cs_port;
                cs_pin = _cs_pin;
            }

            void read_register(uint8_t, uint8_t*, size_t);
            void update_state();
            // uint16_t read_angle();
            // uint16_t read_speed();
            // void update_state();
            // uint16_t write_register(uint16_t address, uint16_t value);
    };
}