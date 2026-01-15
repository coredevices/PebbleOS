/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "hrm.h"

void hrm_init(HRMDevice *dev) {
}

bool hrm_enable(HRMDevice *dev) {
    dev->state->enabled = true;
    return true;
}

void hrm_disable(HRMDevice *dev) {
    dev->state->enabled = false;
}

bool hrm_is_enabled(HRMDevice *dev) {
    return dev->state->enabled;
}