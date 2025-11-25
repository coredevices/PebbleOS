/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file mfg_vibe_app.h
//!
//! Boring test app that vibes 5 times and quits

#include "process_management/pebble_process_md.h"

const PebbleProcessMd* mfg_vibe_app_get_info(void);

