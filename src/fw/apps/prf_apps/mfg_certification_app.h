/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @file mfg_certification_app.h
//!
//! Boring test app that tweaks various hardware levers to create a worst case scenario for
//! certification testing.

#include "process_management/pebble_process_md.h"

const PebbleProcessMd* mfg_certification_app_get_info(void);
