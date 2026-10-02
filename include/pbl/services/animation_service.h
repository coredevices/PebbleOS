/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include "kernel/pebble_tasks.h"

/**
 * @defgroup services_animation_service Animation service
 * @ingroup services
 * @brief System resources used by the applib animation module.
 *
 * Keeps one timer per task (KernelMain and App) that drives the applib animation scheduler. When
 * the timer fires, a callback event running animation_private_timer_callback() with the task's
 * animation state is posted to that task. Only one such event is kept pending per task.
 * @{
 */

/**
 * @brief Schedule the calling task's animation timer.
 *
 * Syscall; only valid from the KernelMain and App tasks.
 *
 * @param ms Delay until the timer fires, in milliseconds.
 */
void animation_service_timer_schedule(uint32_t ms);

/**
 * @brief Acknowledge the event posted by the calling task's animation timer.
 *
 * Allows the next timer expiry to post a new event. Syscall.
 */
void animation_service_timer_event_received(void);

/**
 * @brief Free the animation resources of a task.
 *
 * Called by the process manager on KernelMain when a process exits.
 *
 * @param task Task whose timer is deleted.
 */
void animation_service_cleanup(PebbleTask task);

/** @} */
