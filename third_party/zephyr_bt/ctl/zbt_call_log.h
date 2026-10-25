/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// The calls the host saw, in a settings file: one record, newest first.

#include <zbt/zbt.h>

//! Put @p rec first, dropping the oldest past ZBT_CALL_LOG_MAX. Not from an ISR.
void zbt_call_log_add(const struct zbt_call_record *rec);
