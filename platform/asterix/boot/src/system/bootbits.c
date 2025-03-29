#include "drivers/dbgserial.h"
#include "system/bootbits.h"
#include "system/retained.h"

#include <git_version.auto.h>

#include <inttypes.h>
#include <stdint.h>

#include <nrfx.h>

static const uint32_t s_bootloader_timestamp = GIT_TIMESTAMP;

void boot_bit_init(void) {
  /* FIXME: compute region to be enabled based on __retained_start (or use nrfx helpers) */
  NRF_POWER->RAM[0].POWERSET |= POWER_RAM_POWER_S2RETENTION_On << POWER_RAM_POWER_S2RETENTION_Pos;

  if (!boot_bit_test(BOOT_BIT_INITIALIZED)) {
    retained_write(RTC_BKP_BOOTBIT_DR, BOOT_BIT_INITIALIZED);
  }
}

void boot_bit_set(BootBitValue bit) {
  uint32_t current_value = retained_read(RTC_BKP_BOOTBIT_DR);
  current_value |= bit;
  retained_write(RTC_BKP_BOOTBIT_DR, current_value);
}

void boot_bit_clear(BootBitValue bit) {
  uint32_t current_value = retained_read(RTC_BKP_BOOTBIT_DR);
  current_value &= ~bit;
  retained_write(RTC_BKP_BOOTBIT_DR, current_value);
}

bool boot_bit_test(BootBitValue bit) {
  uint32_t current_value = retained_read(RTC_BKP_BOOTBIT_DR);
  return (current_value & bit);
}

void boot_bit_dump(void) {
  dbgserial_print("Boot bits: ");
  dbgserial_print_hex(retained_read(RTC_BKP_BOOTBIT_DR));
  dbgserial_newline();
}

void boot_version_write(void) {
  if (boot_version_read() == s_bootloader_timestamp) {
    return;
  }
  retained_write(BOOTLOADER_VERSION_REGISTER, s_bootloader_timestamp);
}

uint32_t boot_version_read(void) {
  return retained_read(BOOTLOADER_VERSION_REGISTER);
}
