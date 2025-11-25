/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

bool sdl_app_init(void);
void sdl_app_deinit(void);
void sdl_app_event_loop(void);
