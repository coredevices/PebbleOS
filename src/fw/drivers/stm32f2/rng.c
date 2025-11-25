/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/rng.h"

#include "drivers/periph_config.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"

#define STM32F2_COMPATIBLE
#define STM32F4_COMPATIBLE
#define STM32F7_COMPATIBLE
#include <mcu.h>


bool rng_rand(uint32_t *rand_out) {
#ifdef TARGET_QEMU
  return false;
#endif
  PBL_ASSERTN(rand_out);

  bool success = false;
  // maximum number of seed errors we allow before giving up:
  uint8_t attempts_left = 3;
  uint8_t non_equal_count = 0;
  uint32_t previous_value = 0;

  periph_config_acquire_lock();
  periph_config_enable(RNG, RCC_AHB2Periph_RNG);
  RNG->CR |= RNG_CR_RNGEN;

  while (true) {
    // Poll the status register's ready bit:
    while (attempts_left) {
      const uint32_t status = RNG->SR;
      // Check clock flags, they would indicate programmer error.
      PBL_ASSERTN((status & (RNG_SR_CECS | RNG_SR_CEIS)) == 0);

      // First check the seed error bits:
      // We're checking both the interrupt flag and status flag, it's not very clear from the docs
      // what the right thing to do is.
      if (status & (RNG_SR_SECS | RNG_SR_SEIS)) {
        // When there is a seed error, ST recommends clearing SEI,
        // then disabling / re-enabling the peripheral:
        RNG->SR &= ~RNG_SR_SEIS;
        RNG->CR &= ~RNG_CR_RNGEN;
        RNG->CR |= RNG_CR_RNGEN;

        non_equal_count = 0;
        previous_value = 0;
        --attempts_left;
        continue;
      }
      if (status & RNG_SR_DRDY) {
        break; // The next random number is ready
      }
    }

    if (!attempts_left) {
      break;
    }

    // As per Cory's and the ST reference manual's suggestion: "As required by the FIPS PUB
    // (Federal Information Processing Standard Publication) 140-2, the first random number
    // generated after setting the RNGEN bit should not be used, but saved for comparison with the
    // next generated random number. Each subsequent generated random number has to be compared with
    // the previously generated number. The test fails if any two compared numbers are equal
    // (continuous random number generator test)."
    *rand_out = RNG->DR;
    if (*rand_out != previous_value) {
      ++non_equal_count;
      if (non_equal_count >= 2) {
        success = true;
        break;
      }
    }
    previous_value = *rand_out;
  }

  RNG->CR &= ~RNG_CR_RNGEN;
  periph_config_disable(RNG, RCC_AHB2Periph_RNG);
  periph_config_release_lock();
  return success;
}
