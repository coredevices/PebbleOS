/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// clang-format off
#include "system_resource.h"

#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>
#include <pbl/fs/fs.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/text_resources.h>
#include <kernel/event_loop.h>
#include <kernel/memory_layout.h>
#include <kernel/panic.h>
#include <kernel/pbl_malloc.h>
#include <kernel/util/fw_reset.h>
#include <resource/resource.h>
#include <resource/language_pack.h>
#include <resource/resource_ids.auto.h>
#include <resource/resource_storage.h>
#include <resource/resource_version.auto.h>
#include <shell/prefs.h>
#include <font_resource_table.auto.h>
#include <pbl/kernel/syscall.h>
#include <pbl/kernel/assert.h>
// clang-format on

static void prv_watch_language_packs(void);

void system_resource_init(void) {
  prv_watch_language_packs();
  if (!resource_init_app(SYSTEM_APP, &SYSTEM_RESOURCE_VERSION)) {
    // System resources are missing!
#if defined(CONFIG_IS_BIGBOARD)
    static const uint32_t ERROR_BAD_RESOURCES = 0xfe504505;
    pbl_log(LOG_LEVEL_ERROR, __FILE_NAME__, __LINE__,
            "System resources are missing or corrupt, time to sad watch");
    launcher_panic(ERROR_BAD_RESOURCES);
#else
    PBL_LOG_ERR("System resources are missing or corrupt! Going to PRF");
    fw_reset_into_prf();
#endif
  }
}

bool system_resource_is_valid(void) {
  return resource_init_app(SYSTEM_APP, &SYSTEM_RESOURCE_VERSION);
}

#define NUM_SYSTEM_FONTS ARRAY_LENGTH(s_font_resource_keys)

// Total number of fonts = NUM_SYSTEM_FONTS + 1 for the fallback font
FontInfo s_system_fonts_info_table[NUM_SYSTEM_FONTS + 1] KERNEL_READONLY_DATA;
uint32_t g_font_language_generation KERNEL_READONLY_DATA;

#ifndef CONFIG_RECOVERY_FW
#define NUM_LANGUAGE_FONTS \
  (RESOURCE_ID_LECO_60_BOLD_NUMBERS_AM_PM_EXTENDED - RESOURCE_ID_GOTHIC_14_EXTENDED + 1)
static_assert(NUM_LANGUAGE_FONTS <= 32, "checked bits must fit in a word");

// Extension fonts of a pack, loaded on first use and kept until its file changes.
typedef struct {
  FontInfo fonts[NUM_LANGUAGE_FONTS];
  uint32_t checked;
} LanguageFonts;

typedef enum {
  LanguagePackUnknown = 0,
  LanguagePackAbsent,
  LanguagePackPresent,
} LanguagePackState;

static struct pbl_mutex s_language_fonts_mutex;
static LanguageFonts *s_language_fonts[LANGUAGE_PACK_COUNT];
static LanguagePackState s_language_pack_state[LANGUAGE_PACK_COUNT];
// Bumped when a pack file changes, so a font loaded outside the lock can't be stored stale
static uint32_t s_language_pack_changes[LANGUAGE_PACK_COUNT];

static uint8_t prv_primary_font_slot(void) {
  return shell_prefs_get_language_english() ? 0
                                            : shell_language_pack_slot(shell_prefs_get_language());
}

static void prv_invalidate_system_fonts(void) {
  // Shared fonts reload lazily. A concurrent render may miss a glyph for one frame;
  // the next lookup retries after initialization completes.
  for (size_t i = 0; i < ARRAY_LENGTH(s_system_fonts_info_table); ++i) {
    s_system_fonts_info_table[i].loaded = false;
  }
  // Publish last: a renderer that sees the new generation must find the fonts unloaded, or it
  // would refill its cache from the old ones under the new generation.
  __atomic_store_n(&g_font_language_generation, g_font_language_generation + 1, __ATOMIC_RELEASE);
}

static void prv_language_changed(void *data) {
  // Other packs' fonts stay valid; only the primary slot moved.
  prv_invalidate_system_fonts();
}

static void prv_language_pack_changed(void *data) {
  const uint8_t slot = (uintptr_t)data;
  // Cached entries still point at offsets in the old file.
  resource_uncache(language_pack_resource_id(slot, RESOURCE_ID_STRINGS, RESOURCE_ID_STRINGS),
                   language_pack_resource_id(slot, RESOURCE_ID_LECO_60_BOLD_NUMBERS_AM_PM_EXTENDED,
                                             RESOURCE_ID_STRINGS));
  pbl_mutex_lock(&s_language_fonts_mutex, PBL_FOREVER);
  kernel_free(s_language_fonts[slot]);
  s_language_fonts[slot] = NULL;
  s_language_pack_state[slot] = LanguagePackUnknown;
  ++s_language_pack_changes[slot];
  prv_invalidate_system_fonts();
  pbl_mutex_unlock(&s_language_fonts_mutex);
}

static void prv_language_pack_file_changed(void *data) {
  // PFS holds its lock here, which resource reads take after the resource lock.
  launcher_task_add_callback(prv_language_pack_changed, data);
}

static LanguageFonts *prv_get_language_fonts(uint8_t slot) {
  if (s_language_pack_state[slot] == LanguagePackUnknown) {
    s_language_pack_state[slot] =
        language_pack_is_installed(slot) ? LanguagePackPresent : LanguagePackAbsent;
  }
  if (s_language_pack_state[slot] == LanguagePackAbsent) {
    return NULL;
  }
  if (!s_language_fonts[slot]) {
    s_language_fonts[slot] = kernel_zalloc(sizeof(LanguageFonts));
  }
  return s_language_fonts[slot];
}

static int prv_language_font_index(uint32_t base_resource_id) {
  for (size_t i = 0; i < NUM_SYSTEM_FONTS; ++i) {
    if (s_font_resource_keys[i].resource_id == base_resource_id) {
      const uint32_t extension = s_font_resource_keys[i].extension_id;
      if (extension >= RESOURCE_ID_GOTHIC_14_EXTENDED &&
          extension <= RESOURCE_ID_LECO_60_BOLD_NUMBERS_AM_PM_EXTENDED) {
        return extension - RESOURCE_ID_GOTHIC_14_EXTENDED;
      }
      break;
    }
  }
  return -1;
}
#endif

void system_resource_language_changed(void) {
#ifndef CONFIG_RECOVERY_FW
  // The language changes on any task; the font tables are only ever invalidated from one.
  launcher_task_add_callback(prv_language_changed, NULL);
#endif
}

static void prv_watch_language_packs(void) {
#ifndef CONFIG_RECOVERY_FW
  pbl_mutex_init(&s_language_fonts_mutex);
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    pbl_fs_watch_add(language_pack_filename(slot), PBL_FS_WATCH_ALL, prv_language_pack_file_changed,
                     (void *)(uintptr_t)slot);
  }
#endif
}

PBL_SYSCALL(bool, sys_font_get_language_font, (VAL, uint32_t, base_resource_id),
            (VAL, uint8_t, slot), (OUT, FontInfo *, font_out)) {
#ifndef CONFIG_RECOVERY_FW
  const int index = prv_language_font_index(base_resource_id);
  if (index < 0 || slot >= LANGUAGE_PACK_COUNT || slot == prv_primary_font_slot()) {
    return false;
  }
  const uint32_t bit = 1u << index;
  pbl_mutex_lock(&s_language_fonts_mutex, PBL_FOREVER);
  LanguageFonts *fonts = prv_get_language_fonts(slot);
  const bool checked = fonts && (fonts->checked & bit);
  if (checked) {
    *font_out = fonts->fonts[index];
  }
  const uint32_t changes = s_language_pack_changes[slot];
  pbl_mutex_unlock(&s_language_fonts_mutex);
  if (!fonts) {
    return false;
  }
  if (checked) {
    return font_out->loaded;
  }

  // Validating a large font takes a while; keep the lock free for renders and invalidation.
  FontInfo font = {};
  const uint32_t resource =
      language_pack_resource_id(slot, RESOURCE_ID_GOTHIC_14_EXTENDED + index, RESOURCE_ID_STRINGS);
  font.loaded = text_resources_init_font(SYSTEM_APP, resource, 0, &font);

  pbl_mutex_lock(&s_language_fonts_mutex, PBL_FOREVER);
  const bool current = s_language_pack_changes[slot] == changes && s_language_fonts[slot];
  if (current) {
    s_language_fonts[slot]->fonts[index] = font;
    s_language_fonts[slot]->checked |= bit;
  }
  pbl_mutex_unlock(&s_language_fonts_mutex);
  *font_out = font;
  return current && font.loaded;
#else
  return false;
#endif
}

static GFont prv_load_system_font(const char *font_key) {
  if (font_key == nullptr) {
    PBL_LOG_DBG("GETTING FALLBACK FONT");
    // load fallback font
    if (!s_system_fonts_info_table[NUM_SYSTEM_FONTS].loaded) {
      PBL_ASSERTN(text_resources_init_font(SYSTEM_APP, RESOURCE_ID_FONT_FALLBACK_INTERNAL, 0,
                                           &s_system_fonts_info_table[NUM_SYSTEM_FONTS]));
    }
    return &s_system_fonts_info_table[NUM_SYSTEM_FONTS];
  }

  for (int i = 0; i < (int)NUM_SYSTEM_FONTS; ++i) {
    if (0 == strcmp(font_key, s_font_resource_keys[i].key_name)) {
      FontInfo *fontinfo = &s_system_fonts_info_table[i];
      uint32_t resource = s_font_resource_keys[i].resource_id;
      uint32_t extension = s_font_resource_keys[i].extension_id;
#ifndef CONFIG_RECOVERY_FW
      if (extension) {
        extension =
            language_pack_resource_id(prv_primary_font_slot(), extension, RESOURCE_ID_STRINGS);
      }
#endif
      // if the font has not been initialized yet
      if (!fontinfo->loaded) {
        if (!text_resources_init_font(SYSTEM_APP, resource, extension,
                                      &s_system_fonts_info_table[i])) {
          // Can't initialize the font for some reason
          return nullptr;
        }
        resource_get_and_cache(SYSTEM_APP, resource);
        resource_get_and_cache(SYSTEM_APP, extension);
      }
      return &s_system_fonts_info_table[i];
    }
  }

  // Didn't find the given font, invalid key.
  return nullptr;
}

GFont system_resource_get_font(const char *font_key) {
  GFont result = prv_load_system_font(font_key);
  return result;
}

PBL_SYSCALL(GFont, sys_font_get_system_font, (VAL, const char *, font_key)) {
  if (font_key && from_user) {
    if (!memory_layout_is_cstring_in_region(memory_layout_get_app_region(), font_key, 100) &&
        !memory_layout_is_cstring_in_region(memory_layout_get_microflash_region(), font_key, 100)) {
      PBL_LOG_ERR("Pointer %p not in app or microflash region", font_key);
      pbl_syscall_fail();
    }
  }

  return system_resource_get_font(font_key);
}

static bool prv_is_system_font_info(const FontInfo *fontinfo) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_system_fonts_info_table); ++i) {
    if (fontinfo == &s_system_fonts_info_table[i]) {
      return true;
    }
  }
  return false;
}

PBL_SYSCALL(void, sys_font_reload_font, (VAL, FontInfo *, fontinfo)) {
  if (from_user && !prv_is_system_font_info(fontinfo)) {
    pbl_syscall_fail();
  }

  uint32_t extension = fontinfo->extension.resource_id;
#ifndef CONFIG_RECOVERY_FW
  for (size_t i = 0; i < NUM_SYSTEM_FONTS; ++i) {
    if (fontinfo == &s_system_fonts_info_table[i]) {
      extension = s_font_resource_keys[i].extension_id;
      if (extension) {
        extension =
            language_pack_resource_id(prv_primary_font_slot(), extension, RESOURCE_ID_STRINGS);
      }
      break;
    }
  }
#endif
  text_resources_init_font(fontinfo->base.app_num, fontinfo->base.resource_id, extension, fontinfo);
}
