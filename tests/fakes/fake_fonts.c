/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_fonts.h"

#include "resource/resource_ids.auto.h"
#include "font_resource_keys.auto.h"
#include "applib/fonts/fonts_private.h"
#include "applib/graphics/text_resources.h"
#include "util/math.h"
#include "util/size.h"

#include <string.h>

#include "clar_asserts.h"

typedef struct {
  const char *key;
  uint32_t handle;
  FontInfo font_info;
} FontHelper;

static FontHelper s_font_helpers[] = {
    {.key = FONT_KEY_GOTHIC_14, .handle = RESOURCE_ID_GOTHIC_14},
    {.key = FONT_KEY_GOTHIC_14_BOLD, .handle = RESOURCE_ID_GOTHIC_14_BOLD},
    {.key = FONT_KEY_GOTHIC_18, .handle = RESOURCE_ID_GOTHIC_18},
    {.key = FONT_KEY_GOTHIC_18_BOLD, .handle = RESOURCE_ID_GOTHIC_18_BOLD},
    {.key = FONT_KEY_GOTHIC_24_BOLD, .handle = RESOURCE_ID_GOTHIC_24_BOLD},
    {.key = FONT_KEY_DROID_SERIF_28_BOLD, .handle = RESOURCE_ID_GOTHIC_28_BOLD},
    {.key = FONT_KEY_LECO_20_BOLD_NUMBERS, .handle = RESOURCE_ID_LECO_20_BOLD_NUMBERS},
    {.key = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM, .handle = RESOURCE_ID_LECO_26_BOLD_NUMBERS_AM_PM},
    {.key = FONT_KEY_LECO_32_BOLD_NUMBERS, .handle = RESOURCE_ID_LECO_32_BOLD_NUMBERS},
    {.key = FONT_KEY_LECO_36_BOLD_NUMBERS, .handle = RESOURCE_ID_LECO_36_BOLD_NUMBERS},
    {.key = FONT_KEY_LECO_38_BOLD_NUMBERS, .handle = RESOURCE_ID_LECO_38_BOLD_NUMBERS},
    {.key = FONT_KEY_GOTHIC_14_EMOJI, .handle = RESOURCE_ID_GOTHIC_14_EMOJI},
    {.key = FONT_KEY_GOTHIC_18_EMOJI, .handle = RESOURCE_ID_GOTHIC_18_EMOJI},
    {.key = FONT_KEY_GOTHIC_24, .handle = RESOURCE_ID_GOTHIC_24},
    {.key = FONT_KEY_GOTHIC_24_EMOJI, .handle = RESOURCE_ID_GOTHIC_24_EMOJI},
    {.key = FONT_KEY_GOTHIC_28, .handle = RESOURCE_ID_GOTHIC_28},
    {.key = FONT_KEY_GOTHIC_28_EMOJI, .handle = RESOURCE_ID_GOTHIC_28_EMOJI},
    {.key = FONT_KEY_GOTHIC_28_BOLD, .handle = RESOURCE_ID_GOTHIC_28_BOLD},
    {.key = FONT_KEY_GOTHIC_36, .handle = RESOURCE_ID_GOTHIC_36},
    {.key = FONT_KEY_GOTHIC_36_BOLD, .handle = RESOURCE_ID_GOTHIC_36_BOLD},
#if (PLATFORM_SNOWY || PLATFORM_SPALDING)
    {.key = FONT_KEY_AGENCY_FB_36_NUMBERS_AM_PM, .handle = RESOURCE_ID_AGENCY_FB_36_NUMBERS_AM_PM },
    {.key = FONT_KEY_AGENCY_FB_60_NUMBERS_AM_PM, .handle = RESOURCE_ID_AGENCY_FB_60_NUMBERS_AM_PM },
    {.key = FONT_KEY_AGENCY_FB_60_THIN_NUMBERS_AM_PM, .handle = RESOURCE_ID_AGENCY_FB_60_THIN_NUMBERS_AM_PM },
#elif PLATFORM_ROBERT || PLATFORM_OBELIX
    {.key = FONT_KEY_AGENCY_FB_46_NUMBERS_AM_PM, .handle = RESOURCE_ID_AGENCY_FB_46_NUMBERS_AM_PM },
    {.key = FONT_KEY_AGENCY_FB_88_NUMBERS_AM_PM, .handle = RESOURCE_ID_AGENCY_FB_88_NUMBERS_AM_PM },
    {.key = FONT_KEY_AGENCY_FB_88_THIN_NUMBERS_AM_PM, .handle = RESOURCE_ID_AGENCY_FB_88_THIN_NUMBERS_AM_PM },
#endif
    // add more here as we need more fonts from this module
};

static FontHelper *prv_font_helper_from_font_key(const char *font_key) {
  for (int i = 0; i < ARRAY_LENGTH(s_font_helpers); i++) {
    if (!strcmp(font_key, s_font_helpers[i].key)) {
      return &s_font_helpers[i];
    }
  }
  return NULL;
}

static GFont prv_get_font(const char *font_key) {
  FontHelper *font_helper = prv_font_helper_from_font_key(font_key);
  cl_assert_(font_helper, font_key);
  FontInfo *result = &font_helper->font_info;
  if (!result->loaded) {
    bool init_result = text_resources_init_font(0, font_helper->handle, 0, result);
    cl_assert_(init_result, font_key);
  }

  return (GFont)result;
}

static const struct {
  const char *key_name;
  uint8_t min_height;
} s_emoji_fonts[] = {
    // Keep this sorted in descending order
    { FONT_KEY_GOTHIC_28_EMOJI, 28 },
    { FONT_KEY_GOTHIC_24_EMOJI, 24 },
    { FONT_KEY_GOTHIC_18_EMOJI, 18 },
    { FONT_KEY_GOTHIC_14_EMOJI, 14 },
};

FontInfo *fonts_get_system_emoji_font_for_size(unsigned int font_height) {
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_emoji_fonts); i++) {
    if (font_height >= s_emoji_fonts[i].min_height) {
      return prv_get_font(s_emoji_fonts[i].key_name);
    }
  }
  // Didn't find a suitable emoji font
  return NULL;
}

GFont fonts_get_system_font(const char *font_key) {
  return prv_get_font(font_key);
}

GFont system_resource_get_font(const char *font_key) {
  return prv_get_font(font_key);
}

uint8_t fonts_get_font_height(GFont font) {
  return font->max_height;
}

int16_t fonts_get_font_cap_offset(GFont font) {
  if (!font) {
    return 0;
  }

  // FIXME PBL-25709: Actually use font-specific caps and also provide function for baseline offsets
  return (int16_t)(((int16_t)font->max_height) * 22 / 100);
}
