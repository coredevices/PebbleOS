/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// A minimal system font table: one font with a language pack extension, one without.
static const struct {
  const char *key_name;
  ResourceId resource_id;
  ResourceId extension_id;
} s_font_resource_keys[] = {
  {FONT_KEY_GOTHIC_18, RESOURCE_ID_GOTHIC_18, RESOURCE_ID_GOTHIC_18_EXTENDED},
  {FONT_KEY_GOTHIC_24, RESOURCE_ID_GOTHIC_24, 0},
};
