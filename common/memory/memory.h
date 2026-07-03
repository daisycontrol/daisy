#pragma once
#include "main.h"
#include <cstring>

namespace memory
{

void initialize(size_t _size, uint8_t _settings_page);
void* get_addr();

HAL_StatusTypeDef erase_settings_page(void);
HAL_StatusTypeDef write_record(uint32_t _addr, void* _data, size_t _length);

HAL_StatusTypeDef write_memory(void* _data);

} // memory