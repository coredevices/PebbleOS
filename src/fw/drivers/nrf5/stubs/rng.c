#include "drivers/rng.h"

#include "drivers/periph_config.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"

#define NRF5_COMPATIBLE
#include <mcu.h>


bool rng_rand(uint32_t *rand_out) {
  return false;
}
