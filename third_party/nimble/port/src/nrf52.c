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

#include <system/passert.h>

#undef UNUSED
#include <nrfx.h>

static void (*radio_irq)(void);
static void (*rtc0_irq)(void);
static void (*rng_irq)(void);

void RADIO_IRQHandler(void) {
  if (radio_irq != NULL) {
    radio_irq();
  }
}

void RTC0_IRQHandler(void) {
  if (rtc0_irq != NULL) {
    rtc0_irq();
  }
}

void RNG_IRQHandler(void) {
  if (rng_irq != NULL) {
    rng_irq();
  }
}

void ble_npl_hw_set_isr(int irqn, void (*addr)(void)) {
  switch (irqn) {
    case RADIO_IRQn:
      radio_irq = addr;
      break;
    case RTC0_IRQn:
      rtc0_irq = addr;
      break;
    case RNG_IRQn:
      rng_irq = addr;
      break;
    default:
      WTF;
  }
}
