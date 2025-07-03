/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>

#include <system/passert.h>

#include <FreeRTOS.h>
#include <hal/nrf_clock.h>

static uint8_t prv_refcnt;

void nrf52_clock_hfxo_request(void) {
  portENTER_CRITICAL();

  PBL_ASSERT(prv_refcnt < UINT8_MAX, "HFXO refcount overflow");

  if (prv_refcnt == 0U && !nrf_clock_hf_is_running(NRF_CLOCK, NRF_CLOCK_HFCLK_HIGH_ACCURACY)) {
    nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);
    nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
    while (!nrf_clock_event_check(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED)) {
    }
  }

  prv_refcnt++;

  portEXIT_CRITICAL();
}

void nrf52_clock_hfxo_release(void) {
  portENTER_CRITICAL();

  PBL_ASSERT(prv_refcnt != 0U, "HFXO refcount underflow");

  prv_refcnt--;
  if (prv_refcnt == 0U) {
    nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTOP);
  }

  portEXIT_CRITICAL();
}
