/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/notifications/action_chaining_window.h"

void action_chaining_window_push(WindowStack *window_stack, const char *title,
                                 TimelineItemActionGroup *action_group,
                                 ActionChainingMenuSelectCb select_cb,
                                 void *select_cb_context,
                                 ActionChainingMenuClosedCb closed_cb,
                                 void *closed_cb_context) {
  return;
}
