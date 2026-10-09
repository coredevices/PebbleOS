/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/button_id.h>

//! Input key code of a button.
uint16_t input_buttons_code(ButtonId id);
