/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Sends the battery measurement to all subscribed & connected devices.
void pbl_bt_bas_handle_update(uint8_t percent);
