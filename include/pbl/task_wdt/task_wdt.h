/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "pbl/kernel/thread.h"

//! Task watchdog: a software watchdog with one channel per thread that has
//! to keep proving it makes progress. A channel that is not fed within its
//! timeout is reported, its owner gets a chance to recover through the
//! channel callback, and the system resets with a core dump once the grace
//! period runs out. See docs/architecture/task_watchdog.md.

//! Runs on the watchdog thread each time an expired channel is checked,
//! before the system resets. It may try to unblock the thread.
//! @return a pointer naming the work the thread was running, recorded in the
//! reboot reason, or NULL.
typedef void *(*pbl_task_wdt_callback_t)(int channel_id, void *user_data);

//! Starts the watchdog thread. Every channel starts with a full timeout.
void pbl_task_wdt_init(void);

//! Adds a channel that @p thread (NULL: the calling thread) must feed at
//! least every @p timeout_ms. The thread must delete the channel before it
//! exits.
//! @return the channel id, or -ENOMEM when every channel is in use.
int pbl_task_wdt_add(struct pbl_thread *thread, uint32_t timeout_ms,
                     pbl_task_wdt_callback_t callback, void *user_data);

//! @return 0, or -EINVAL for a channel that is not in use.
int pbl_task_wdt_delete(int channel_id);

//! @return 0, or -EINVAL for a channel that is not in use.
int pbl_task_wdt_feed(int channel_id);

//! Feeds the channels of the calling thread, if it has any.
void pbl_task_wdt_feed_self(void);

//! Feeds the channels of @p thread; a no-op for NULL.
void pbl_task_wdt_feed_thread(struct pbl_thread *thread);

void pbl_task_wdt_feed_all(void);

//! Keeps every channel fed for @p timeout_ms, or until pbl_task_wdt_resume() when
//! 0. A new call replaces the suspension in progress.
void pbl_task_wdt_suspend(uint32_t timeout_ms);

//! Ends a suspension; every channel restarts with a full timeout.
void pbl_task_wdt_resume(void);
