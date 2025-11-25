/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

#include <stdbool.h>

//! Initialize the HRM
void hrm_init(HRMDevice *dev);

//! Enable the HRM
void hrm_enable(HRMDevice *dev);

//! Disable the HRM
void hrm_disable(HRMDevice *dev);

//! Checks whether or not the HRM is enabled
bool hrm_is_enabled(HRMDevice *dev);
