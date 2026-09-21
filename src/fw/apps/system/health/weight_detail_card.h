/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "data.h"

#include "applib/ui/window.h"

Window *health_weight_detail_card_create(HealthData *health_data);
void health_weight_detail_card_destroy(Window *window);
