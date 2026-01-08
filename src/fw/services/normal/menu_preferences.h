/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/vibes/vibe_intensity.h"

#include <stdbool.h>

void menu_preferences_init(void);

bool menu_preferences_get_scroll_wrap_around(void);

void menu_preferences_set_scroll_wrap_around(bool set);

bool menu_preferences_get_scroll_vibe_on_wrap_around(void);

void menu_preferences_set_scroll_vibe_on_wrap_around(bool set);

bool menu_preferences_get_scroll_vibe_on_blocked(void);

void menu_preferences_set_scroll_vibe_on_blocked(bool set);
