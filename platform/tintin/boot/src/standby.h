/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Setup wakeup sources and put board into standby mode
void enter_standby_mode(void);

//! Check to make sure we should be leaving standby mode
bool should_leave_standby_mode(void);

//! Return functionality to normal after standby mode
void leave_standby_mode(void);
