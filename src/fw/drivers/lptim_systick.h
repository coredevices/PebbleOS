/*
 * Copyright 2025 Core Devices LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

void lptim_systick_init(void);

bool lptim_systick_is_initialized(void);

void lptim_systick_enable(void);

void lptim_systick_pause(void);

void lptim_systick_resume(void);

void lptim_systick_tickless_idle(uint32_t ticks_from_now);

uint32_t lptim_systick_get_elapsed_ticks(void);
