/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define APP_IDLE_TIMEOUT_LAUNCHER_MS  (30 * 1000)
#define APP_IDLE_TIMEOUT_MENU_MS      (60 * 1000)
#define APP_IDLE_TIMEOUT_BLUETOOTH_MS (3 * 60 * 1000)

//! Start using the idle timeout for the current app. After timeout_ms without activity the
//! watchface is launched.
void app_idle_timeout_start(uint32_t timeout_ms);

//! Change the timeout of the running idle timeout and restart it. No-op if it isn't running.
void app_idle_timeout_set_duration(uint32_t timeout_ms);

//! Stop using the idle timeout for the current app. This is safe to call even if the idle timeout
//! wasn't running.
void app_idle_timeout_stop(void);

//! Pause the idle timeout for the current app. This is safe to call even if the idle timeout wasn't
//! running previously.
void app_idle_timeout_pause(void);

//! Resume the idle timeout for the current app. This is safe to call even if the idle timeout
//! wasn't running previously.
void app_idle_timeout_resume(void);

//! Reset the timeout. Call this whenever there is activity that should prevent the idle timeout
//! from firing. This is safe to call even if the idle timeout wasn't running previously.
void app_idle_timeout_refresh(void);

//! Halt the idle timeout while a finger is on the touchscreen. Unlike pause/resume, this state
//! composes with the focus-driven pause. Safe to call even if the idle timeout isn't running.
void app_idle_timeout_touch_down(void);

//! Restart the idle timeout when the finger lifts off. Safe to call even if the idle timeout
//! isn't running or no touch-down was observed.
void app_idle_timeout_touch_up(void);
