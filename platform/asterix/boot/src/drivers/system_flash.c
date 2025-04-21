#include "drivers/system_flash.h"

#include <hal/nrf_nvmc.h>

#include "drivers/dbgserial.h"
#include "util/misc.h"

#define FLASH_SECTOR_SIZE 0x1000
#define FLASH_SECTOR_COUNT 256

static inline bool addr_is_word_aligned(uint32_t addr) { return (addr & 0x3u) == 0u; }

int prv_get_sector_num_for_address(uint32_t address) {
  if (address > FLASH_SECTOR_COUNT * FLASH_SECTOR_SIZE) {
    dbgserial_print("address ");
    dbgserial_print_hex(address);
    dbgserial_putstr(" is outside system flash");
    return -1;
  }
  for (size_t i = 0; i < FLASH_SECTOR_COUNT - 1; ++i) {
    if (FLASH_SECTOR_SIZE * i <= address && address < FLASH_SECTOR_SIZE * (i + 1)) {
      return i;
    }
  }
  return FLASH_SECTOR_COUNT - 1;
}

bool system_flash_erase(uint32_t address, size_t length, SystemFlashProgressCb progress_callback,
                        void *progress_context) {
  if (length == 0) {
    // Nothing to do
    return true;
  }

  if (!addr_is_word_aligned(address)) {
    dbgserial_putstr("system_flash_erase: address not word aligned");
    return false;
  }

  int first_sector = prv_get_sector_num_for_address(address);
  int last_sector = prv_get_sector_num_for_address(address + length - 1);

  if (first_sector < 0 || last_sector < 0) {
    return false;
  }
  int count = last_sector - first_sector + 1;
  if (progress_callback) {
    progress_callback(0, count, progress_context);
  }

  nrf_nvmc_mode_set(NRF_NVMC, NRF_NVMC_MODE_ERASE);
  for (int sector = first_sector; sector <= last_sector; ++sector) {
    nrf_nvmc_page_erase_start(NRF_NVMC, sector * FLASH_SECTOR_SIZE);
    while (!nrf_nvmc_ready_check(NRF_NVMC)) {
      // Wait for the erase to complete
    }
    if (progress_callback) {
      progress_callback(sector - first_sector + 1, count, progress_context);
    }
  }
  nrf_nvmc_mode_set(NRF_NVMC, NRF_NVMC_MODE_READONLY);
  return true;
}

bool system_flash_write(uint32_t address, const void *data, size_t length,
                        SystemFlashProgressCb progress_callback, void *progress_context) {
  uint32_t aligned;
  uint8_t last;

  if (!addr_is_word_aligned(address)) {
    dbgserial_putstr("system_flash_write: address not word aligned");
    return false;
  }

  last = length % 4U;
  aligned = length - last;

  nrf_nvmc_mode_set(NRF_NVMC, NRF_NVMC_MODE_WRITE);

  const uint8_t *data_array = data;
  for (uint32_t i = 0; i < aligned; i += 4) {
    while (!nrf_nvmc_ready_check(NRF_NVMC)) {
      // Wait for the write to be ready
    }

    nrf_nvmc_word_write(address + i, *(uint32_t *)&data_array[i]);
    if (progress_callback && i % 128 == 0) {
      progress_callback(i / 128, aligned / 128, progress_context);
    }
  }

  if (last != 0U) {
    uint32_t val = 0xFFFFFFFFU;

    while (!nrf_nvmc_ready_check(NRF_NVMC)) {
      // Wait for the write to be ready
    }

    memcpy(&val, &data_array[aligned], last);
    nrf_nvmc_word_write(address + aligned, val);
  }

  nrf_nvmc_mode_set(NRF_NVMC, NRF_NVMC_MODE_READONLY);

  return true;
}
