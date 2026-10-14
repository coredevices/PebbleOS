/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/drivers/rng.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_rng.h>

#define CACHE_SIZE 32U

static uint8_t s_cache[CACHE_SIZE];
static volatile uint8_t s_count;
static uint8_t s_in;
static uint8_t s_out;
static bool s_inited;

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(NRF52_RNG_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_RNG_BASE + offset);
}

static void prv_rng_irq_handler(void) {
  if (prv_getreg(NRF52_RNG_EVENTS_VALRDY_OFFSET) == 0U) {
    return;
  }

  prv_putreg(NRF52_RNG_EVENTS_VALRDY_OFFSET, 0U);
  // Read back so the clear lands before the handler returns.
  (void)prv_getreg(NRF52_RNG_EVENTS_VALRDY_OFFSET);

  s_cache[s_in] = prv_getreg(NRF52_RNG_VALUE_OFFSET) & RNG_VALUE_MASK;
  s_in = (s_in + 1U) % CACHE_SIZE;
  s_count++;

  if (s_count == CACHE_SIZE) {
    prv_putreg(NRF52_RNG_TASKS_STOP_OFFSET, 1U);
  }
}

PBL_IRQ_CONNECT(RNG, 7, prv_rng_irq_handler, , 0);

static void prv_init(void) {
  prv_putreg(NRF52_RNG_CONFIG_OFFSET, RNG_CONFIG_DERCEN);
  prv_putreg(NRF52_RNG_EVENTS_VALRDY_OFFSET, 0U);
  prv_putreg(NRF52_RNG_INTENSET_OFFSET, RNG_INT_VALRDY);
  pbl_irq_enable(PBL_IRQN(RNG));

  s_inited = true;
}

bool rng_rand(uint32_t *rand_out) {
  uint8_t *out = (uint8_t *)rand_out;
  size_t n = 0U;

  while (n < sizeof(*rand_out)) {
    pbl_irq_lock();

    if (!s_inited) {
      prv_init();
    }

    while ((s_count > 0U) && (n < sizeof(*rand_out))) {
      out[n++] = s_cache[s_out];
      s_out = (s_out + 1U) % CACHE_SIZE;
      s_count--;
    }

    prv_putreg(NRF52_RNG_TASKS_START_OFFSET, 1U);

    pbl_irq_unlock();
  }

  return true;
}
