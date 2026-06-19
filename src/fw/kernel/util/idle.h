/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Enable or disable idle
void idle_set_enabled(bool enable);

//! Check whether we are permitted to go idle.
bool idle_is_allowed(void);

