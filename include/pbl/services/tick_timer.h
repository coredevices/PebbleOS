/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/pebble_tasks.h"

/**
 * @defgroup services_tick_timer Tick timer
 * @ingroup services
 * @brief Source of the once-per-second @c PEBBLE_TICK_EVENT.
 *
 * The event is only generated while at least one subscriber exists.
 * @{
 */

/**
 * @brief Add a tick event subscriber.
 *
 * @param task Subscribing task, currently unused.
 */
void tick_timer_add_subscriber(PebbleTask task);

/**
 * @brief Remove a tick event subscriber added with tick_timer_add_subscriber().
 *
 * @param task Subscribing task, currently unused.
 */
void tick_timer_remove_subscriber(PebbleTask task);

/** @} */
