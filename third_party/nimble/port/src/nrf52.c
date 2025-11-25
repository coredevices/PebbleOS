/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

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
