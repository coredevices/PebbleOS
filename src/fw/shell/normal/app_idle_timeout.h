/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Start using the idle timeout for the current app.
void app_idle_timeout_start(void);

//! Stop using the idle timeout for the current app. This is safe to call even if the idle timeout wasn't running.
void app_idle_timeout_stop(void);

//! Pause the idle timeout for the current app. This is safe to call even if the idle timeout wasn't running
//! previously.
void app_idle_timeout_pause(void);

//! Resume the idle timeout for the current app. This is safe to call even if the idle timeout wasn't running
//! previously.
void app_idle_timeout_resume(void);

//! Reset the timeout. Call this whenever there is activity that should prevent the idle timeout from firing. This
//! is safe to call even if the idle timeout wasn't running previously.
void app_idle_timeout_refresh(void);

