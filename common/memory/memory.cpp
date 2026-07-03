#include "memory.h"


#define SETTINGS_BANK        FLASH_BANK_1

using namespace memory;

uint32_t flash_bank = FLASH_BANK_1;
uint32_t settings_page = 0;
uint32_t settings_page_size = FLASH_PAGE_SIZE;
uint32_t settings_page_addr = 0;
uint32_t settings_record_size_aligned = 0;
size_t   settings_length = 0;

void memory::initialize(size_t _size, uint8_t _settings_page) {
    settings_page = _settings_page;
    settings_page_addr = (0x08000000UL + (settings_page * FLASH_PAGE_SIZE));
    settings_record_size_aligned = ((_size + 7u) & ~7u);
    settings_length = _size;
}

HAL_StatusTypeDef memory::erase_settings_page(void){

    // TODO: Do this automatically based on board?
    if(settings_page < 63){
        return HAL_ERROR;
    }

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = SETTINGS_BANK,
        .Page = settings_page,
        .NbPages = 1
    };
    uint32_t page_error = 0;
    return HAL_FLASHEx_Erase(&erase, &page_error);
}

HAL_StatusTypeDef memory::write_record(uint32_t _addr, void* _data, size_t _length) {
  uint8_t buf[settings_record_size_aligned];
  memset(buf, 0xFF, sizeof(buf));
  memcpy(buf, _data, _length);

  for (uint32_t offset = 0; offset < sizeof(buf); offset += 8) {
    uint64_t dword;
    memcpy(&dword, &buf[offset], 8);
    HAL_StatusTypeDef status = HAL_FLASH_Program(
      FLASH_TYPEPROGRAM_DOUBLEWORD,
       _addr + offset,
       dword
    );
    if(status != HAL_OK){
      return status;
    }
  }

  return HAL_OK;
}

HAL_StatusTypeDef memory::write_memory(void* _data){
    HAL_StatusTypeDef ret;

    __disable_irq();
    HAL_FLASH_Unlock();
    ret = erase_settings_page();
    ret = write_record(settings_page_addr, _data, settings_length);
    HAL_FLASH_Lock();
    __enable_irq();

    return ret;
}