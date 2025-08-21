#include "drivers/mcu.h"

#define CMSIS_COMPATIBLE
#include <mcu.h>

StatusCode mcu_get_serial(void *buf, size_t *buf_sz) {
  return E_DOES_NOT_EXIST;
}

uint32_t mcu_cycles_to_milliseconds(uint64_t cpu_ticks) {
  return ((cpu_ticks * 1000) / SystemCoreClock);
}
