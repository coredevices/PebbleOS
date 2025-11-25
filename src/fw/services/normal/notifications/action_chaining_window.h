/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/window_stack.h"
#include "services/normal/timeline/item.h"

typedef void (*ActionChainingMenuSelectCb)(Window *chaining_window,
                                           TimelineItemAction *action, void *context);
typedef void (*ActionChainingMenuClosedCb)(void *context);

void action_chaining_window_push(WindowStack *window_stack, const char *title,
                                 TimelineItemActionGroup *action_group,
                                 ActionChainingMenuSelectCb select_cb,
                                 void *select_cb_context,
                                 ActionChainingMenuClosedCb closed_cb,
                                 void *closed_cb_context);
