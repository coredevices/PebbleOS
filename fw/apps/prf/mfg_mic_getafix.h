/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

//! Get the process metadata for the MFG Microphone test app
//! This test records from both microphones, performs FFT analysis,
//! detects peak frequency around 1kHz, and reports PASS/FAIL
const PebbleProcessMd *mfg_mic_getafix_app_get_info(void);
