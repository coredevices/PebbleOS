/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Perform any required link-maintenance tasks before pulse_init.
//!
//! This function should be called as early in boot as possible,
//! preferably as soon as dbgserial output has been initialized.
void pulse_early_init(void);

//! Initialize multitasking PULSE
void pulse_init(void);
