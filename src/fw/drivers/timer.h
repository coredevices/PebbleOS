/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include "board/board.h"

uint16_t timer_find_prescaler(const TimerConfig *timer, uint32_t frequency);
