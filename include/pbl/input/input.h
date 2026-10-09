/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/kernel/section.h>

/**
 * @defgroup input Input
 * @ingroup subsys
 * @brief Input events from drivers to the listeners that consume them.
 *
 * Drivers report events with pbl_input_report() and know nothing about who consumes them.
 * Listeners are defined with PBL_INPUT_CALLBACK_DEFINE() next to the code they drive, collected
 * at link time, and called synchronously in the context of the report, which is an ISR for
 * buttons. Listeners must therefore not block, and must check pbl_in_isr() before using APIs that
 * are not ISR-safe.
 *
 * A report is a group of events closed by one with @c sync set, such as a touch sample:
 *
 * @code{.c}
 * pbl_input_report_key(PBL_INPUT_BTN_TOUCH, true, false);
 * pbl_input_report_abs(PBL_INPUT_ABS_X, x, false);
 * pbl_input_report_abs(PBL_INPUT_ABS_Y, y, true);
 * @endcode
 *
 * @code{.c}
 * static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
 *   if (evt->type == PBL_INPUT_EV_KEY && evt->code == PBL_INPUT_KEY_BACK && evt->value) {
 *     ...
 *   }
 * }
 *
 * PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, NULL);
 * @endcode
 * @{
 */

/** @brief Event type. */
enum pbl_input_type : uint8_t {
  /** Key or button, @c value 1 when pressed and 0 when released. */
  PBL_INPUT_EV_KEY,
  /** Absolute axis, @c value in its own units (pixels for touch). */
  PBL_INPUT_EV_ABS,
  /** Gesture recognized by the device, @c value unused. */
  PBL_INPUT_EV_GES,
};

/** @brief @ref PBL_INPUT_EV_KEY codes. */
enum : uint16_t {
  PBL_INPUT_KEY_BACK,
  PBL_INPUT_KEY_UP,
  PBL_INPUT_KEY_SELECT,
  PBL_INPUT_KEY_DOWN,
  /** Touch contact. */
  PBL_INPUT_BTN_TOUCH,
};

/** @brief @ref PBL_INPUT_EV_ABS codes. */
enum : uint16_t {
  PBL_INPUT_ABS_X,
  PBL_INPUT_ABS_Y,
};

/** @brief @ref PBL_INPUT_EV_GES codes, located at the report's @ref PBL_INPUT_ABS_X and Y. */
enum : uint16_t {
  PBL_INPUT_GES_TAP,
  PBL_INPUT_GES_DOUBLE_TAP,
  PBL_INPUT_GES_PALM,
};

/** @brief Input event. */
struct pbl_input_event {
  /** Type. */
  enum pbl_input_type type;
  /** Last event of a report. */
  bool sync;
  /** Code, depending on @ref type. */
  uint16_t code;
  /** Value, depending on @ref type. */
  int32_t value;
};

/** @brief Listener of input events. */
struct pbl_input_callback {
  /** Called for every event, in the context of the report. */
  void (*callback)(const struct pbl_input_event *evt, void *user_data);
  /** Data passed to @ref callback. */
  void *user_data;
};

/**
 * @brief Define a listener of every input event.
 *
 * @param _callback Function called for every event.
 * @param _user_data Data passed to @p _callback.
 */
#ifdef PBL_NO_LINKER_SCRIPT
#define PBL_INPUT_CALLBACK_DEFINE(_callback, _user_data)                         \
  static const struct pbl_input_callback pbl_input_callback_##_callback PBL_USED \
  PBL_UNSORTED_SECTION(pbl_input_cb) = {.callback = (_callback), .user_data = (_user_data)}
#else
#define PBL_INPUT_CALLBACK_DEFINE(_callback, _user_data)                                        \
  static const struct pbl_input_callback pbl_input_callback_##_callback PBL_USED PBL_ALIGNED(4) \
      PBL_SECTION(".pbl_input_callbacks." #_callback) = {                                       \
        .callback = (_callback),                                                                \
        .user_data = (_user_data)                                                               \
  }
#endif

/**
 * @brief Report an input event to every listener.
 *
 * ISR-safe. The listeners run before this returns.
 *
 * @param type Type.
 * @param code Code, depending on @p type.
 * @param value Value, depending on @p type.
 * @param sync Whether the event ends a report.
 */
void pbl_input_report(enum pbl_input_type type, uint16_t code, int32_t value, bool sync);

/**
 * @brief Report a key event.
 *
 * @param code @ref PBL_INPUT_EV_KEY code.
 * @param pressed Whether the key is pressed.
 * @param sync Whether the event ends a report.
 */
static inline void pbl_input_report_key(uint16_t code, bool pressed, bool sync) {
  pbl_input_report(PBL_INPUT_EV_KEY, code, pressed ? 1 : 0, sync);
}

/**
 * @brief Report an absolute axis event.
 *
 * @param code @ref PBL_INPUT_EV_ABS code.
 * @param value Position.
 * @param sync Whether the event ends a report.
 */
static inline void pbl_input_report_abs(uint16_t code, int32_t value, bool sync) {
  pbl_input_report(PBL_INPUT_EV_ABS, code, value, sync);
}

/**
 * @brief Report a gesture event.
 *
 * @param code @ref PBL_INPUT_EV_GES code.
 * @param sync Whether the event ends a report.
 */
static inline void pbl_input_report_ges(uint16_t code, bool sync) {
  pbl_input_report(PBL_INPUT_EV_GES, code, 0, sync);
}

/** @} */
