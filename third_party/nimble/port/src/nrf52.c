/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/irq.h>
#include <system/passert.h>

#undef UNUSED
#include <nrfx.h>

static void (*radio_irq)(void);
static void (*rtc0_irq)(void);
static void (*rng_irq)(void);

PBL_IRQ_DIRECT(RADIO, 5, 0) {
  if (radio_irq != NULL) {
    radio_irq();
  }
}

PBL_IRQ_DIRECT(RTC0, 7, 0) {
  if (rtc0_irq != NULL) {
    rtc0_irq();
  }
}

PBL_IRQ_DIRECT(RNG, 7, 0) {
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
