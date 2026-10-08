/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/irq.h>

#include <system/passert.h>

#undef UNUSED
#include <nrfx.h>

static void (*radio_irq)(void);
static void (*rtc0_irq)(void);
#ifndef CONFIG_NIMBLE_TRNG
static void (*rng_irq)(void);
#endif

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

#ifndef CONFIG_NIMBLE_TRNG
PBL_IRQ_DIRECT(RNG, 7, 0) {
  if (rng_irq != NULL) {
    rng_irq();
  }
}
#endif

void ble_npl_hw_set_isr(int irqn, void (*addr)(void)) {
  switch (irqn) {
    case RADIO_IRQn:
      radio_irq = addr;
      break;
    case RTC0_IRQn:
      rtc0_irq = addr;
      break;
#ifndef CONFIG_NIMBLE_TRNG
    case RNG_IRQn:
      rng_irq = addr;
      break;
#endif
    default:
      WTF;
  }
}
