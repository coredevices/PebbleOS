/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <time.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup AlarmService
//! \brief Read-only access to the wearer's configured alarms.
//!
//! The AlarmService lets an app find out whether the wearer has an alarm set,
//! which is useful for showing an alarm indicator on a watchface. The system
//! supports up to 10 configured alarms. This API is read-only: apps cannot
//! create, modify, or delete the wearer's alarms.
//!   @{

//! Peek at the next scheduled, enabled alarm.
//! @param timestamp_out Must point to a valid `time_t`. If an enabled alarm
//!        exists, it is set to the UTC time at which that alarm fires.
//! @return true if at least one enabled alarm is scheduled, false otherwise.
bool alarm_service_peek_next(time_t *timestamp_out);

//!   @} // group AlarmService
//! @} // group Foundation
