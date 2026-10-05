/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/rng.h>
#include <pbl/kernel/irq.h>

#include <nrfx.h>
#include <hal/nrf_rng.h>

#define CACHE_SIZE 32U

static uint8_t s_cache[CACHE_SIZE];
static volatile uint8_t s_count;
static uint8_t s_in;
static uint8_t s_out;
static bool s_inited;

static void prv_rng_irq_handler(void) {
  if (!nrf_rng_event_check(NRF_RNG, NRF_RNG_EVENT_VALRDY)) {
    return;
  }

  nrf_rng_event_clear(NRF_RNG, NRF_RNG_EVENT_VALRDY);

  s_cache[s_in] = nrf_rng_random_value_get(NRF_RNG);
  s_in = (s_in + 1U) % CACHE_SIZE;
  s_count++;

  if (s_count == CACHE_SIZE) {
    nrf_rng_task_trigger(NRF_RNG, NRF_RNG_TASK_STOP);
  }
}

PBL_IRQ_CONNECT(RNG, 7, prv_rng_irq_handler, , 0);

static void prv_init(void) {
  nrf_rng_error_correction_enable(NRF_RNG);
  nrf_rng_event_clear(NRF_RNG, NRF_RNG_EVENT_VALRDY);
  nrf_rng_int_enable(NRF_RNG, NRF_RNG_INT_VALRDY_MASK);
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

    nrf_rng_task_trigger(NRF_RNG, NRF_RNG_TASK_START);

    pbl_irq_unlock();
  }

  return true;
}
