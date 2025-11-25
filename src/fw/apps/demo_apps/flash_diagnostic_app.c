/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "flash_diagnostic_app.h"

#include <stdio.h>

#include "applib/app.h"
#include "applib/ui/simple_menu_layer.h"
#include "applib/ui/ui.h"
#include "applib/ui/window.h"
#include "drivers/flash.h"
#include "drivers/task_watchdog.h"
#include "flash_region/flash_region.h"
#include "kernel/pbl_malloc.h"
#include "kernel/pebble_tasks.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "resource/resource_storage_flash.h"
#include "system/logging.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"
#include "util/size.h"

#define NUM_REGIONS ARRAY_LENGTH(s_flash_regions)

#define FILE_WRITE_STRESS (NUM_REGIONS)
#define FILE_SUBSECTOR_STRESS (FILE_WRITE_STRESS + 1)
#define NUM_STRESS_TESTS (FILE_SUBSECTOR_STRESS - NUM_REGIONS + 1)

#define NUM_MENU_ITEMS (NUM_REGIONS + NUM_STRESS_TESTS)

struct Region {
  char *name;
  uint32_t begin;
  uint32_t end;
};

static struct Region s_flash_regions[] = {
  {
    "System Resources",
    FLASH_REGION_SYSTEM_RESOURCES_BANK_0_BEGIN,
    FLASH_REGION_SYSTEM_RESOURCES_BANK_0_END
  },
  {
    "System Resources",
    FLASH_REGION_SYSTEM_RESOURCES_BANK_1_BEGIN,
    FLASH_REGION_SYSTEM_RESOURCES_BANK_1_END
  },
  {
    "File System",
    FLASH_REGION_FILESYSTEM_BEGIN,
    FLASH_REGION_FILESYSTEM_END
  },
};

typedef struct {
  Window window;
  SimpleMenuLayer menu_layer;
  SimpleMenuSection menu_section;
  SimpleMenuItem menu_items[NUM_MENU_ITEMS];
} FlashDiagAppData;

typedef struct {
  Window window;
  TextLayer *text_layer;
  int stress_iteration;
  int stress_index;
} FlashStressWindow;


static bool check_region_erased(struct Region region) {
  PBL_LOG_SYNC(LOG_LEVEL_INFO, "Checking Erase ...");
  bool success = true;
  for (uint32_t i = region.begin; i < region.end; i += sizeof(uint32_t)) {
    uint32_t read = 0;
    flash_read_bytes((uint8_t *)&read, i, sizeof(read));
    if (read != 0xffffffff) {
      PBL_LOG_SYNC(LOG_LEVEL_INFO, ">>>> Address 0x%lx failed to erase: 0x%lx", i, read);
      success = false;
    }
  }

  return success;
}

// region: region to check (and possibly write)
// use_rand: write random values
// perform_writes: perform writes if true, else see if the region reads as 0
static bool check_region_write(struct Region region, bool use_rand,
    bool perform_writes) {
  bool success = true;
  uint32_t write_rand = (use_rand && perform_writes) ? rand() : 0;

  PBL_LOG_SYNC(LOG_LEVEL_INFO, "%sChecking 0x%lx over 0x%lx 0x%lx",
      (perform_writes) ? "Writing and " : "", write_rand, region.begin,
      region.end);

  for (uint32_t i = region.begin; i < region.end; i += sizeof(uint32_t)) {
    uint32_t write = write_rand;
    uint32_t read = 0xffff;
    if (perform_writes) {
      flash_write_bytes((uint8_t *)&write, i, sizeof(write));
    }
    flash_read_bytes((uint8_t *)&read, i, sizeof(read));
    if (read != write) {
      PBL_LOG_SYNC(LOG_LEVEL_INFO, ">>>> Address 0x%lx failed to write: 0x%lx 0x%lx",
          i, read, write);
      success = false;
    }
  }

  return success;
}

// Writes 0's to the first half of a flash sector and confirms that everything
// reads as 0.  Then uses 8 subsector erases to erase the second half of the
// sector. Then re-reads the first half to see if any bits have flipped
static bool check_subsector_bitflip(struct Region region) {
#if !CAPABILITY_USE_PARALLEL_FLASH
  bool success = true;

  if (((region.end - region.begin) % (64 * 1024)) != 0) {
    PBL_LOG(LOG_LEVEL_WARNING, "Test only works on 64k aligned regions");
    return (false);
  }

  const uint32_t block_size = 64 * 1024;
  const uint32_t write_size = 32 * 1024;
  const uint32_t subsector_size = 4 * 1024;

  for (uint32_t i = region.begin; i < region.end; i += block_size) {

    struct Region write_region;
    write_region.begin = i;
    write_region.end = i + write_size;

    if (!check_region_write(write_region, false, true)) {
      success = false;
      break;
    }

    uint32_t subsec_begin = block_size - write_size;
    PBL_ASSERTN((subsec_begin % (32*1024)) == 0);

    for (uint32_t subsec = subsec_begin; subsec < block_size;
        subsec += subsector_size) {
      uint32_t erase = subsec + i;
      PBL_ASSERTN((erase % (4 * 1024)) == 0);
      PBL_LOG_SYNC(LOG_LEVEL_INFO, "Subsector Erase of 0x%lx", erase);
      flash_erase_subsector_blocking(erase);
    }

    if (!check_region_write(write_region, false, false)) {
      success = false;
      break;
    }
    psleep(5);
  }

  return (success);
#else
  PBL_LOG_SYNC(LOG_LEVEL_INFO, "Test not supported for parallel flash");
  return (false);
#endif
}

static void menu_select_callback(int index, void *data) {
  struct Region region = s_flash_regions[index];
  PBL_LOG(LOG_LEVEL_INFO, ">>>> Erase %s", region.name);
  flash_region_erase_optimal_range(region.begin, region.begin, region.end, region.end);
  PBL_LOG(LOG_LEVEL_INFO, ">>>> Checking '%s' is erased", region.name);
  check_region_erased(region);
  PBL_LOG(LOG_LEVEL_INFO, ">>>> Checking '%s' can write", region.name);
  check_region_write(region, false, true);
  PBL_LOG(LOG_LEVEL_INFO, ">>>> Done!");
}

FlashStressWindow stress_data;
static bool abort_stress_test;
static void update_text(int iter, int tot, bool failed) {
  static char status[50];

  snprintf(status, sizeof(status), "%d / %d %s", iter, tot,
           failed ? "Failed Out" : "Complete");
  text_layer_set_text(stress_data.text_layer, (char *)status);
}

static void app_timer_cb(void *data) {
  static const int num_stress_iters = 1000;
  struct Region region = s_flash_regions[2];
  PBL_LOG(LOG_LEVEL_INFO, ">>>> %s %d", "Test Loop", stress_data.stress_iteration);

  PBL_LOG(LOG_LEVEL_INFO, "Erasing 0x%lx to 0x%lx", region.begin, region.end);
  flash_region_erase_optimal_range(region.begin, region.begin, region.end, region.end);

  bool failed = true;
  if (stress_data.stress_index == FILE_WRITE_STRESS) {
    failed = (!check_region_erased(region) || !check_region_write(region, true, true));
  } else if (stress_data.stress_index == FILE_SUBSECTOR_STRESS) {
    failed = (!check_region_erased(region) || !check_subsector_bitflip(region));
  } else {
    PBL_LOG(LOG_LEVEL_WARNING, "Unknown stress test %d!", stress_data.stress_index);
  }

  if (!abort_stress_test) {
    update_text(++stress_data.stress_iteration, num_stress_iters, failed);

    if (!failed && (stress_data.stress_iteration < num_stress_iters)) {
      app_timer_register(1000, app_timer_cb, NULL); // allow for animation to complete
    } else { // clean up state
      flash_region_erase_optimal_range(region.begin, region.begin, region.end, region.end);
    }
  }
}

static void stress_window_load(Window *data) {
  Layer *layer = window_get_root_layer(data);
  const int16_t width = layer->frame.size.w - ACTION_BAR_WIDTH - 3;

  stress_data.text_layer = text_layer_create(GRect(4, 44, width, 60));
  TextLayer *text_layer = stress_data.text_layer;
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_background_color(text_layer, GColorClear);
  layer_add_child(layer, text_layer_get_layer(text_layer));

  text_layer_set_text(stress_data.text_layer, "Starting Stress Test");
  abort_stress_test = false;
  app_timer_register(500, app_timer_cb, NULL);
};

static void stress_window_unload(Window *data) {
  abort_stress_test = true;
}

static void file_system_stress_callback(int index, void *data) {
  stress_data.stress_iteration = 0;
  stress_data.stress_index = index;
  window_init(&stress_data.window, WINDOW_NAME("Stress Test"));
  window_set_user_data(&stress_data.window, &stress_data);
  window_set_window_handlers(&stress_data.window, &(WindowHandlers) {
      .load = stress_window_load,
      .unload = stress_window_unload
  });
  app_window_stack_push(&stress_data.window, true);
}

static void populate_menu(SimpleMenuSection *menu_section, SimpleMenuItem *menu_items) {
  for (unsigned int i = 0; i < NUM_REGIONS; ++i) {
    menu_items[i] = (SimpleMenuItem) {
      .title = s_flash_regions[i].name,
      .callback = menu_select_callback,
    };
  }
  menu_items[FILE_WRITE_STRESS] = (SimpleMenuItem) {
    .title = "File Stress",
    .callback = file_system_stress_callback,
  };
  menu_items[FILE_SUBSECTOR_STRESS] = (SimpleMenuItem) {
    .title = "Subsector Stress",
    .callback = file_system_stress_callback,
  };

  menu_section->num_items = NUM_MENU_ITEMS;
  menu_section->items = menu_items;
  menu_section->title = "Flash Regions";
}

static void prv_window_load(Window *window) {
  FlashDiagAppData *data = window_get_user_data(window);
  populate_menu(&data->menu_section, data->menu_items);
  Layer *root_layer = window_get_root_layer(window);
  const GRect *bounds = &root_layer->bounds;
  simple_menu_layer_init(&data->menu_layer, bounds, window, &data->menu_section, 1, NULL);
  layer_add_child(root_layer, simple_menu_layer_get_layer(&data->menu_layer));
}

static void push_window(FlashDiagAppData *data) {
  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Flash Diagnostic"));
  window_set_user_data(window, data);
  window_set_window_handlers(window, &(WindowHandlers) {
    .load = prv_window_load,
  });
  const bool animated = true;
  app_window_stack_push(window, animated);
}

////////////////////
// App boilerplate
static void handle_init(void) {
  FlashDiagAppData *data = (FlashDiagAppData*) app_malloc_check(sizeof(FlashDiagAppData));
  if (data == NULL) {
    PBL_CROAK("Out of memory");
  }
  app_state_set_user_data(data);
  push_window(data);
}

static void handle_deinit(void) {
  FlashDiagAppData *data = app_state_get_user_data();
  simple_menu_layer_deinit(&data->menu_layer);
  app_free(data);
}

static void s_main(void) {
  if (resource_storage_flash_get_unused_bank()->begin ==
      s_flash_regions[0].begin) {
    s_flash_regions[0].name = "Unused Resources";
  } else {
    s_flash_regions[1].name = "Unused Resources";
  }

  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* flash_diagnostic_app_get_info() {
  static const PebbleProcessMdSystem s_flash_diagnostic_app_info = {
    .common.main_func = s_main,
    .name = "Flash Diagnostic"
  };
  return (const PebbleProcessMd*) &s_flash_diagnostic_app_info;
}
