#include "drivers/mcu.h"

#define NRF5_COMPATIBLE
#include <mcu.h>

#include <drivers/nrfx_common.h>
#include <soc/nrfx_coredep.h>

static uint32_t did = 0;
const uint32_t* mcu_get_serial(void) {
  return &did;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  return ((cpu_ticks * 1000) / SystemCoreClock);
}

void pwr_enable_wakeup(bool enable) {
}

void pwr_flash_power_down_stop_mode(bool power_down) {
}

void pwr_access_backup_domain(bool enable_access) {
}
