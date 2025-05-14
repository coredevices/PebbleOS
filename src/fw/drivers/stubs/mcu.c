#include "drivers/mcu.h"

#define CMSIS_COMPATIBLE
#include <mcu.h>

static const uint32_t serial = 0U;

const uint32_t* mcu_get_serial(void) {
  return &serial;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  return ((cpu_ticks * 1000) / SystemCoreClock);
}
