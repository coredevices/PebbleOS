/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef const struct MicDevice MicDevice;
static MicDevice *const MIC = (void *)0;

typedef const struct HRMDevice HRMDevice;
static HRMDevice *const HRM = (void *)0;

static const struct AudioDevice *const AUDIO = (void *)0;
