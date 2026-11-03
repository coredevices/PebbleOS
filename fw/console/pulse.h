/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Resynchronize the host's link state, right after the early console is up.
void pulse_early_init(void);

//! Initialize multitasking PULSE
void pulse_init(void);
