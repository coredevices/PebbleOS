/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef CONFIG_MFG

#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/timer.h>

#include <system/bootbits.h>
#include <system/reboot_reason.h>
#include <system/reset.h>

// Must stay well below the PMIC's back button reset, ~8-11 s, so that a user trying to reset
// does not shut the watch down instead.
#define RESET_KEYS_HOLD_MS 5000U

enum {
  KEY_BACK = 1U << 0,
  KEY_SELECT = 1U << 1,
  KEY_UP = 1U << 2,
};

#define RESET_KEYS (KEY_BACK | KEY_SELECT)

static uint8_t s_held;

static void prv_held(struct pbl_timer *timer) {
  pbl_irq_lock();

  // Up held as well forces PRF.
  const bool force_prf = (s_held & KEY_UP) != 0U;
  if (force_prf) {
    boot_bit_set(BOOT_BIT_FORCE_PRF);
  }

  RebootReason reason = {
    .code = force_prf ? RebootReasonCode_PrfResetButtonsHeld : RebootReasonCode_ResetButtonsHeld,
  };
  reboot_reason_set(&reason);

  // This back door must work: no orderly shutdown.
  system_hard_reset();
}

static struct pbl_timer s_timer = PBL_TIMER_INITIALIZER(prv_held);

static uint8_t prv_key(uint16_t code) {
  switch (code) {
    case PBL_INPUT_KEY_BACK:
      return KEY_BACK;
    case PBL_INPUT_KEY_SELECT:
      return KEY_SELECT;
    case PBL_INPUT_KEY_UP:
      return KEY_UP;
    default:
      return 0U;
  }
}

static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  uint8_t key;

  if (evt->type != PBL_INPUT_EV_KEY) {
    return;
  }

  key = prv_key(evt->code);
  if (key == 0U) {
    return;
  }

  if (evt->value) {
    s_held |= key;
  } else {
    s_held &= ~key;
  }

  if ((s_held & RESET_KEYS) != RESET_KEYS) {
    pbl_timer_stop(&s_timer);
  } else if (!pbl_timer_is_active(&s_timer)) {
    pbl_timer_start(&s_timer, PBL_MSEC(RESET_KEYS_HOLD_MS));
  }
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);

#endif
