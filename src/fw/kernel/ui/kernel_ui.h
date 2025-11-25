/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/ui/content_indicator_private.h"
#include "services/normal/timeline/timeline_actions.h"

void kernel_ui_init(void);

GContext* kernel_ui_get_graphics_context(void);

GContext *graphics_context_get_current_context(void);

ContentIndicatorsBuffer *kernel_ui_get_content_indicators_buffer(void);

ContentIndicatorsBuffer *content_indicator_get_current_buffer(void);

TimelineItemActionSource kernel_ui_get_current_timeline_item_action_source(void);
void kernel_ui_set_current_timeline_item_action_source(TimelineItemActionSource current_source);
