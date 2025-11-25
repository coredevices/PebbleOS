/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fonts.h"
#include "fonts_private.h"

#include "applib/applib_malloc.auto.h"
#include "applib/applib_resource.h"
#include "applib/graphics/text.h"
#include "applib/graphics/text_resources.h"
#include "process_management/app_manager.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "syscall/syscall.h"
#include "system/passert.h"
#include "system/logging.h"
#include "util/list.h"
#include "util/size.h"

#include <string.h>

GFont fonts_get_fallback_font(void) {
  // No font key for the fallback font
  return sys_font_get_system_font(NULL);
}

GFont fonts_get_system_font(const char *font_key) {
  static const char bitham_alias[] = "RESOURCE_ID_GOTHAM";
  static const char bitham_prefix[] = "RESOURCE_ID_BITHAM";
  static const size_t bitham_alias_len = sizeof(bitham_alias)-1;
  static const size_t bitham_prefix_len = sizeof(bitham_prefix)-1;

  GFont res = sys_font_get_system_font(font_key);

  // maybe they wanted a renamed font
  if (NULL == res && 0 == strncmp(font_key, bitham_alias, bitham_alias_len)) {
    char new_font_key[bitham_prefix_len - bitham_alias_len + strlen(font_key) + 1];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(new_font_key, bitham_prefix, bitham_prefix_len);
#pragma GCC diagnostic pop
    strcpy(new_font_key+bitham_prefix_len, font_key+bitham_alias_len);
    // let's try again
    res = sys_font_get_system_font(new_font_key);
  }

  if (NULL == res) {
    PBL_LOG(LOG_LEVEL_DEBUG, "Getting fallback font instead");
    res = fonts_get_fallback_font();
    PBL_ASSERTN(res);
  }

  return res;
}

GFont fonts_load_custom_font(ResHandle handle) {
  GFont res = fonts_load_custom_font_system(sys_get_current_resource_num(), (uint32_t)handle);
  if (res == NULL) {
    PBL_LOG(LOG_LEVEL_WARNING, "Getting fallback font instead");
    res = sys_font_get_system_font("RESOURCE_ID_GOTHIC_14");
  }
  return res;
}

GFont fonts_load_custom_font_system(ResAppNum app_num, uint32_t resource_id) {
  if (resource_id == 0) {
    PBL_LOG(LOG_LEVEL_ERROR, "Tried to load a font from a NULL resource");
    return NULL;
  }

  FontInfo *font_info = applib_type_malloc(FontInfo);
  if (font_info == NULL) {
    PBL_LOG(LOG_LEVEL_ERROR, "Couldn't malloc space for new font");
    return NULL;
  }

  bool result = text_resources_init_font(app_num, resource_id,
                                         0 /* extended resource */, font_info);

  if (!result) {
    // couldn't init the font
    applib_free(font_info);
    return NULL;
  }

  return font_info;
}

void fonts_unload_custom_font(GFont font) {
  // fonts_load_custom_font can return gothic 14 if loading their font didn't
  // work for whatever reason. We don't let the app know that it failed, so it makes sense that
  // they'll later try to unload this returned pointer at a later point. We don't actually want
  // to free this, so just no-op.
  if (font == sys_font_get_system_font("RESOURCE_ID_GOTHIC_14")) {
    return;
  }

  FontInfo *font_info = (FontInfo*) font;
  applib_free(font_info);
}

#if !RECOVERY_FW
static const struct {
  const char *key_name;
  uint8_t height;
} s_emoji_fonts[] = {
    // Keep this sorted in descending order
  { FONT_KEY_GOTHIC_28_EMOJI, 28 },
  { FONT_KEY_GOTHIC_24_EMOJI, 24 },
  { FONT_KEY_GOTHIC_18_EMOJI, 18 },
  { FONT_KEY_GOTHIC_14_EMOJI, 14 },
};

FontInfo *fonts_get_system_emoji_font_for_size(unsigned int font_height) {
  for (uint32_t i = 0; i < ARRAY_LENGTH(s_emoji_fonts); i++) {
    if (font_height == s_emoji_fonts[i].height) {
      return sys_font_get_system_font(s_emoji_fonts[i].key_name);
    }
  }
  // Didn't find a suitable emoji font
  return NULL;
}
#endif

uint8_t fonts_get_font_height(GFont font) {
  FontInfo* fontinfo = (FontInfo*) font;
  return fontinfo->max_height;
}

int16_t fonts_get_font_cap_offset(GFont font) {
  if (!font) {
    return 0;
  }

  // FIXME PBL-25709: Actually use font-specific caps and also provide function for baseline offsets
  return (int16_t)(((int16_t)font->max_height) * 22 / 100);
}
