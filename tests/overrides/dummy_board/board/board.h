/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef const struct HRMDevice HRMDevice;
static HRMDevice *const HRM = (void *)0;

static const struct pbl_uart *const CONSOLE_UART = (void *)0;
