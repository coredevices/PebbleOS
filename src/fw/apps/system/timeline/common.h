/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/timeline.h"

#define TIMELINE_NUM_VISIBLE_ITEMS (2)

#define TIMELINE_PAST_COLOR PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorLightGray)
#define TIMELINE_FUTURE_COLOR GColorVividCerulean
#define TIMELINE_DOT_COLOR GColorBlack

typedef TimelineIterDirection TimelineDirection;
