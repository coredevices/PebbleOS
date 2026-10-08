/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/ui/action_toggle.h>

/**
 * @defgroup services_notifications_do_not_disturb_toggle Do Not Disturb toggle
 * @ingroup services_notifications
 * @brief Quiet Time toggle dialog.
 * @{
 */

/**
 * @brief Push the Quiet Time toggle.
 *
 * Sets manual DND to the opposite of the current DND active state, which also overrides scheduled
 * and smart DND.
 *
 * @param prompt Whether to ask for confirmation first.
 * @param set_exit_reason Whether to set the app exit reason when the toggle completes.
 */
void do_not_disturb_toggle_push(ActionTogglePrompt prompt, bool set_exit_reason);

/** @} */
