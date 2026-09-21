/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef void (*WeightEntrySavedCallback)(uint16_t weight_dag, void *context);

void health_weight_entry_window_push(uint16_t initial_weight_dag,
                                     WeightEntrySavedCallback saved_callback, void *context);
