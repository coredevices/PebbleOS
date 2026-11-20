/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

#include <pbl/services/alarms/alarm.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup services_alarms_alarm_pin Alarm pins
 * @ingroup services_alarms
 * @brief Timeline pins showing alarms.
 *
 * Pins are created from the watch with the alarms data source as parent, and an action that
 * opens the alarm in the alarms app.
 * @{
 */

/**
 * @brief Add a pin for an alarm to the timeline.
 *
 * @param alarm_time Time of the pin.
 * @param id Alarm the pin belongs to.
 * @param type Alarm type, selects the pin title.
 * @param kind Alarm recurrence, shown on the pin.
 * @param[out] uuid_out ID of the new pin, may be NULL.
 * @retval 0 Pin added.
 * @retval -ENOMEM The pin could not be created.
 * @return Other pin database errors.
 */
int alarm_pin_add(time_t alarm_time, AlarmId id, AlarmType type, AlarmKind kind, Uuid *uuid_out);

/**
 * @brief Remove an alarm pin from the timeline.
 *
 * @param alarm_id ID of the pin to remove.
 */
void alarm_pin_remove(Uuid *alarm_id);

/**
 * @brief Remove future alarm pins that no alarm tracks anymore.
 *
 * @param now Current time; pins before it are kept.
 * @param tracked IDs of the pins still owned by alarms.
 * @param tracked_count Number of entries in @p tracked.
 * @retval 0 All untracked future pins were removed.
 * @return Otherwise, the pin database error.
 */
int alarm_pin_remove_untracked_future(time_t now, const Uuid *tracked, size_t tracked_count);

/** @} */
