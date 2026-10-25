/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/compositor/compositor.h>

#include <kernel/events.h>

void watchface_init(void);

void watchface_handle_button_event(PebbleEvent *e);

#ifdef CONFIG_TOUCH
void watchface_handle_touch_event(const TouchEvent *event);
#endif

AppInstallId watchface_get_default_install_id(void);

void watchface_launch_default(const CompositorTransition *animation);

void watchface_set_timeline_peek_enabled(bool visible);

void watchface_reset_click_manager(void);
