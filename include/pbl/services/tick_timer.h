/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/pebble_tasks.h>

/**
 * @defgroup services_tick_timer Tick timer
 * @ingroup services
 * @brief Source of the @c PEBBLE_TICK_EVENT.
 *
 * The event is only generated while at least one subscriber exists: every second while a task
 * subscribed through sys_tick_timer_subscribe() with @c SECOND_UNIT, every minute otherwise.
 * @{
 */

/**
 * @brief Add a tick event subscriber.
 *
 * @param task Subscribing task.
 */
void tick_timer_add_subscriber(PebbleTask task);

/**
 * @brief Remove a tick event subscriber added with tick_timer_add_subscriber().
 *
 * @param task Subscribing task.
 */
void tick_timer_remove_subscriber(PebbleTask task);

/** @} */
