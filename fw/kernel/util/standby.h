/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <system/reboot_reason.h>

//! Power the watch off.
//!
//! Only returns if standby can't be entered and nothing has been torn down yet.
//!
//! @retval -EBUSY USB power is connected.
int enter_standby(RebootReasonCode reason);
