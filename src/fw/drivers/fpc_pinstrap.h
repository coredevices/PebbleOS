/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file fpc_pinstrap.h
//!
//! This file implements an API to read the pinstrap values for an attached FPC (flexible printed
//! circuit). These values are used for version identification so we can figure out which version
//! of FPC is connected to our main PCB.

#define FPC_PINSTRAP_NOT_AVAILABLE 0xff

//! @return uint8_t a value between 0 and 8 to represent the pinstrap value. If the pinstrap
//!                 value isn't valid on this platform, FPC_PINSTRAP_NOT_AVAILABLE is returned.
uint8_t fpc_pinstrap_get_value(void);
