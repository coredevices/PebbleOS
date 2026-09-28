/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! Switches the debug serial to its shell. Ctrl-D switches it back to logs.
void shell_dbgserial_start_from_isr(bool *should_context_switch);

void shell_dbgserial_handle_char(char c, bool *should_context_switch);
