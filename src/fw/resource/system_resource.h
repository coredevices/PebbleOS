/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/fonts/fonts.h"

void system_resource_init(void);

bool system_resource_is_valid(void);

GFont system_resource_get_font(const char *font_key);

