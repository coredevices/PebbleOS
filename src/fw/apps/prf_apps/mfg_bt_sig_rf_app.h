/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file mfg_bt_sig_rf_app.h
//!
//! Boring test app that puts us into bt_sig_rf mode for testing.

#include "process_management/pebble_process_md.h"

const PebbleProcessMd* mfg_bt_sig_rf_app_get_info(void);

