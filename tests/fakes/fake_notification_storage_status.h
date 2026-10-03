/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/uuid.h"

#include <stdbool.h>
#include <stdint.h>

//! Forgets every status write so far.
void fake_notification_storage_status_reset(void);

//! Whether notification_storage_set_status() has been called since the last reset.
bool fake_notification_storage_status_was_set(void);

//! The status bits written to the last id passed to notification_storage_set_status(), added up
//! the way flash adds them up.
uint8_t fake_notification_storage_status_get_last(Uuid *id_out);
