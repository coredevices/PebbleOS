/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/rtc.h"
#include "drivers/touch/touch_sensor.h"
#include "pbl/services/system_task.h"
#include "pbl/services/touch/touch.h"
#include "util/math.h"

#include "FreeRTOS.h"

#include <cmsis_core.h>
#include <stdbool.h>
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// QEMU touch register offsets (must match pebble-touch device)
#define TOUCH_STATE     0x00
#define TOUCH_X         0x04
#define TOUCH_Y         0x08
#define TOUCH_INTCTRL   0x0C
#define TOUCH_INTSTAT   0x10

#define INT_TOUCH_EVENT (1u << 0)

// The QEMU pebble-touch device only reports raw finger position, so synthesize
// Tap/DoubleTap/Swipe* from the down/up stream to mirror what the on-device
// CST816 controller emits.
#define SWIPE_THRESHOLD_PX     30
#define TAP_MAX_DURATION_MS    300
#define DOUBLE_TAP_WINDOW_MS   400
#define DOUBLE_TAP_DIST_PX     30

static bool s_callback_scheduled = false;

static bool s_finger_down = false;
static int16_t s_start_x;
static int16_t s_start_y;
static int16_t s_last_x;
static int16_t s_last_y;
static RtcTicks s_start_ticks;

static int16_t s_last_tap_x;
static int16_t s_last_tap_y;
static RtcTicks s_last_tap_ticks;  // 0 means no pending tap

static uint32_t prv_ticks_to_ms(RtcTicks ticks) {
  return (uint32_t)((ticks * 1000u) / RTC_TICKS_HZ);
}

static void prv_recognize_gesture(int16_t end_x, int16_t end_y, RtcTicks end_ticks) {
  const int32_t dx = (int32_t)end_x - s_start_x;
  const int32_t dy = (int32_t)end_y - s_start_y;
  const int32_t abs_dx = ABS(dx);
  const int32_t abs_dy = ABS(dy);

  if (abs_dx >= SWIPE_THRESHOLD_PX || abs_dy >= SWIPE_THRESHOLD_PX) {
    TouchGesture g;
    if (abs_dx >= abs_dy) {
      g = (dx > 0) ? TouchGesture_SwipeRight : TouchGesture_SwipeLeft;
    } else {
      g = (dy > 0) ? TouchGesture_SwipeDown : TouchGesture_SwipeUp;
    }
    touch_handle_gesture(g, end_x, end_y);
    s_last_tap_ticks = 0;
    return;
  }

  const uint32_t duration_ms = prv_ticks_to_ms(end_ticks - s_start_ticks);
  if (duration_ms > TAP_MAX_DURATION_MS) {
    s_last_tap_ticks = 0;
    return;
  }

  if (s_last_tap_ticks != 0) {
    const uint32_t since_last_ms = prv_ticks_to_ms(end_ticks - s_last_tap_ticks);
    const int32_t tap_dx = (int32_t)end_x - s_last_tap_x;
    const int32_t tap_dy = (int32_t)end_y - s_last_tap_y;
    if (since_last_ms <= DOUBLE_TAP_WINDOW_MS &&
        ABS(tap_dx) <= DOUBLE_TAP_DIST_PX &&
        ABS(tap_dy) <= DOUBLE_TAP_DIST_PX) {
      touch_handle_gesture(TouchGesture_DoubleTap, end_x, end_y);
      s_last_tap_ticks = 0;
      return;
    }
  }

  touch_handle_gesture(TouchGesture_Tap, end_x, end_y);
  s_last_tap_x = end_x;
  s_last_tap_y = end_y;
  s_last_tap_ticks = end_ticks;
}

static void prv_process_touch_update(void *unused) {
  s_callback_scheduled = false;

  const uint32_t base = QEMU_TOUCH_BASE;
  const uint32_t state = REG32(base + TOUCH_STATE);
  const int16_t x = (int16_t)REG32(base + TOUCH_X);
  const int16_t y = (int16_t)REG32(base + TOUCH_Y);

  if (state & INT_TOUCH_EVENT) {
    if (!s_finger_down) {
      s_finger_down = true;
      s_start_x = x;
      s_start_y = y;
      s_start_ticks = rtc_get_ticks();
    }
    s_last_x = x;
    s_last_y = y;
    touch_handle_update(TouchState_FingerDown, x, y);
  } else {
    if (s_finger_down) {
      s_finger_down = false;
      // The lift IRQ may report a stale or reset position; use the last
      // observed down-position so a drag still registers as a swipe even
      // when intermediate motion IRQs were coalesced away.
      prv_recognize_gesture(s_last_x, s_last_y, rtc_get_ticks());
    }
    touch_handle_update(TouchState_FingerUp, x, y);
  }
}

void TOUCH_IRQHandler(void) {
  REG32(QEMU_TOUCH_BASE + TOUCH_INTSTAT) = INT_TOUCH_EVENT;

  bool should_context_switch = false;
  if (!s_callback_scheduled) {
    if (system_task_add_callback_from_isr(prv_process_touch_update, NULL,
                                          &should_context_switch)) {
      s_callback_scheduled = true;
    }
  }
  portEND_SWITCHING_ISR(should_context_switch);
}

void touch_sensor_init(void) {
  const uint32_t base = QEMU_TOUCH_BASE;

  REG32(base + TOUCH_INTSTAT) = INT_TOUCH_EVENT;
  REG32(base + TOUCH_INTCTRL) = INT_TOUCH_EVENT;

  NVIC_SetPriority(TOUCH_IRQn, 6);
  NVIC_EnableIRQ(TOUCH_IRQn);
}

void touch_sensor_set_enabled(bool enabled) {
  REG32(QEMU_TOUCH_BASE + TOUCH_INTCTRL) = enabled ? INT_TOUCH_EVENT : 0;
}
