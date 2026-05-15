/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "test_mpu_violation.h"

#include "applib/app.h"
#include "applib/app_timer.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/click.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "font_resource_keys.auto.h"
#include "kernel/pbl_malloc.h"
#include "kernel/memory_layout.h"
#include "process_state/app_state/app_state.h"

#include <stdio.h>

// Demo app that deliberately runs a series of memory accesses that the
// MPU is supposed to deny for the unprivileged App task. Use up/down to
// cycle through tests; press select to run the highlighted test. The
// expected outcome for every test is a MemManage fault: the kernel
// kills the App task and the launcher reclaims the screen. If the app
// stays alive long enough to render "SURVIVED!" the MPU let the access
// through -- that's the regression signal.
//
// SimpleMenuLayer would have been a nicer UI but it touches kernel data
// not accessible to an unprivileged App task, so we use a plain Window
// + TextLayers and raw click handlers.
//
// Linker symbols used for region-specific probes.
extern const uint32_t __WORKER_RAM__[];
extern const uint32_t __FLASH_start__[];
extern const uint32_t __APP_RAM__[];
extern const uint32_t __kernel_main_stack_start__[];
#ifdef MICRO_FAMILY_SF32LB52
extern const uint32_t __ramfunc_start[];
#endif

#ifndef MPU_TYPE_ARMV8M
KERNEL_READONLY_DATA static uint32_t s_ro_bss_probe;
#endif

typedef enum {
  TestKind_WorkerRamWrite,
  TestKind_WorkerRamRead,
  TestKind_KernelRamWrite,
  TestKind_KernelRamRead,
#ifdef MICRO_FAMILY_SF32LB52
  TestKind_RamfuncWrite,
  TestKind_RamfuncRead,
#endif
#ifndef MPU_TYPE_ARMV8M
  TestKind_RoBssWrite,
#endif
  TestKind_FlashWrite,
#ifndef MPU_TYPE_ARMV8M
  TestKind_StackGuardWrite,
#endif
  TestKind_StackOverflow,
  TestKindCount,
} TestKind;

static const char *const s_test_titles[TestKindCount] = {
    [TestKind_WorkerRamWrite] = "Worker RAM W",
    [TestKind_WorkerRamRead] = "Worker RAM R",
    [TestKind_KernelRamWrite] = "Kernel RAM W",
    [TestKind_KernelRamRead] = "Kernel RAM R",
#ifdef MICRO_FAMILY_SF32LB52
    [TestKind_RamfuncWrite] = "Ramfunc W",
    [TestKind_RamfuncRead] = "Ramfunc R",
#endif
#ifndef MPU_TYPE_ARMV8M
    [TestKind_RoBssWrite] = "RO BSS W",
#endif
    [TestKind_FlashWrite] = "Flash W",
#ifndef MPU_TYPE_ARMV8M
    [TestKind_StackGuardWrite] = "Stack guard W",
#endif
    [TestKind_StackOverflow] = "Stack overflow",
};

typedef struct {
  Window window;
  TextLayer header_text;
  TextLayer selection_text;
  TextLayer hint_text;

  char selection_buffer[40];

  int selected_index;
  bool test_running;
} AppData;

// Deeply-recursing helper for the stack-overflow test. The non-trivial
// return value plus the volatile local prevents the compiler from
// turning this into a tail call (which would not consume stack).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
static uint32_t __attribute__((noinline)) prv_overflow_recurse(uint32_t depth) {
  volatile uint8_t big_local[128];
  big_local[0] = (uint8_t)depth;
  return prv_overflow_recurse(depth + 1) + big_local[0];
}
#pragma GCC diagnostic pop

static void prv_run_test(TestKind kind) {
  switch (kind) {
    case TestKind_WorkerRamWrite: {
      volatile uint32_t *p = (volatile uint32_t *)__WORKER_RAM__;
      *p = 0xDEADBEEF;
      break;
    }
    case TestKind_WorkerRamRead: {
      volatile uint32_t *p = (volatile uint32_t *)__WORKER_RAM__;
      volatile uint32_t v = *p;
      (void)v;
      break;
    }
    case TestKind_KernelRamWrite: {
      // Bottom of the kernel main stack -- well inside kernel RAM and
      // not covered by any region the App task can see.
      volatile uint32_t *p = (volatile uint32_t *)__kernel_main_stack_start__;
      *p = 0xDEADBEEF;
      break;
    }
    case TestKind_KernelRamRead: {
      volatile uint32_t *p = (volatile uint32_t *)__kernel_main_stack_start__;
      volatile uint32_t v = *p;
      (void)v;
      break;
    }
#ifdef MICRO_FAMILY_SF32LB52
    case TestKind_RamfuncWrite: {
      volatile uint32_t *p = (volatile uint32_t *)__ramfunc_start;
      *p = 0xDEADBEEF;
      break;
    }
    case TestKind_RamfuncRead: {
      volatile uint32_t *p = (volatile uint32_t *)__ramfunc_start;
      volatile uint32_t v = *p;
      (void)v;
      break;
    }
#endif
#ifndef MPU_TYPE_ARMV8M
    case TestKind_RoBssWrite: {
      // ARMv7-M maps this as priv RW + user RO, so unprivileged writes fault.
      volatile uint32_t *p = &s_ro_bss_probe;
      *p = 0xDEADBEEF;
      break;
    }
#endif
    case TestKind_FlashWrite: {
      volatile uint32_t *p = (volatile uint32_t *)__FLASH_start__;
      *p = 0xDEADBEEF;
      break;
    }
#ifndef MPU_TYPE_ARMV8M
    case TestKind_StackGuardWrite: {
      // Direct store to the bottom 32 B of App RAM. On ARMv7-M the per-task
      // stack-guard MPU region blocks this; ARMv8-M uses PSPLIM instead, so
      // this scenario is omitted there.
      volatile uint32_t *p = (volatile uint32_t *)__APP_RAM__;
      *p = 0xDEADBEEF;
      break;
    }
#endif
    case TestKind_StackOverflow:
      // Eventually trips PSPLIM (ARMv8-M) or the MPU stack-guard region
      // (ARMv7-M), since each call frame consumes ~128 B.
      (void)prv_overflow_recurse(0);
      break;
    case TestKindCount:
      break;
  }
}

static void prv_redraw_selection(AppData *data) {
  snprintf(data->selection_buffer, sizeof(data->selection_buffer),
           "[%d/%d]\n%s", data->selected_index + 1, TestKindCount,
           s_test_titles[data->selected_index]);
  text_layer_set_text(&data->selection_text, data->selection_buffer);
  layer_mark_dirty(text_layer_get_layer(&data->selection_text));
}

static void prv_attempt(void *cb_data) {
  AppData *data = cb_data;
  prv_run_test((TestKind)data->selected_index);

  // Reaching this point means no fault. Surface that prominently --
  // the previous "TESTING..." text gets replaced so the survival is
  // obvious in a screenshot.
  text_layer_set_text(&data->selection_text, "SURVIVED!\n(MPU MISS)");
  layer_mark_dirty(text_layer_get_layer(&data->selection_text));
  data->test_running = false;
}

static void prv_up_click(ClickRecognizerRef rec, void *ctx) {
  (void)rec;
  AppData *data = ctx;
  if (data->test_running) {
    return;
  }
  data->selected_index =
      (data->selected_index + TestKindCount - 1) % TestKindCount;
  prv_redraw_selection(data);
}

static void prv_down_click(ClickRecognizerRef rec, void *ctx) {
  (void)rec;
  AppData *data = ctx;
  if (data->test_running) {
    return;
  }
  data->selected_index = (data->selected_index + 1) % TestKindCount;
  prv_redraw_selection(data);
}

static void prv_select_click(ClickRecognizerRef rec, void *ctx) {
  (void)rec;
  AppData *data = ctx;
  if (data->test_running) {
    return;
  }
  data->test_running = true;
  text_layer_set_text(&data->selection_text, "TESTING...");
  layer_mark_dirty(text_layer_get_layer(&data->selection_text));
  // Wait a tick so the "TESTING..." frame is visible before the access.
  app_timer_register(300, prv_attempt, data);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
}

static void prv_window_load(Window *window) {
  AppData *data = window_get_user_data(window);

  GRect bounds;
  layer_get_bounds(window_get_root_layer(window), &bounds);

  // Header at the top: "MPU violation".
  GRect header_frame = bounds;
  header_frame.size.h = 24;
  text_layer_init(&data->header_text, &header_frame);
  text_layer_set_font(&data->header_text,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(&data->header_text, GTextAlignmentCenter);
  text_layer_set_text(&data->header_text, "MPU violation");
  layer_add_child(window_get_root_layer(window),
                  text_layer_get_layer(&data->header_text));

  // Selection display in the middle.
  GRect selection_frame = bounds;
  selection_frame.origin.y = 32;
  selection_frame.size.h = bounds.size.h - 64;
  text_layer_init(&data->selection_text, &selection_frame);
  text_layer_set_font(&data->selection_text,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(&data->selection_text, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window),
                  text_layer_get_layer(&data->selection_text));

  // Hint at the bottom: button mapping.
  GRect hint_frame = bounds;
  hint_frame.origin.y = bounds.size.h - 24;
  hint_frame.size.h = 24;
  text_layer_init(&data->hint_text, &hint_frame);
  text_layer_set_font(&data->hint_text,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(&data->hint_text, GTextAlignmentCenter);
  text_layer_set_text(&data->hint_text, "Up/Dn cycle, Sel run");
  layer_add_child(window_get_root_layer(window),
                  text_layer_get_layer(&data->hint_text));

  prv_redraw_selection(data);
}

static void prv_handle_init(void) {
  AppData *app_data = app_malloc_check(sizeof(AppData));
  app_data->selected_index = 0;
  app_data->test_running = false;
  app_state_set_user_data(app_data);

  window_init(&app_data->window, WINDOW_NAME("test_mpu_violation"));
  window_set_user_data(&app_data->window, app_data);
  window_set_window_handlers(&app_data->window, &(WindowHandlers) {
      .load = prv_window_load,
  });
  // Pass app_data as the click context so handlers don't have to reach
  // for a static (which would live in kernel BSS and fault on write).
  window_set_click_config_provider_with_context(&app_data->window,
                                                prv_click_config, app_data);

  app_window_stack_push(&app_data->window, true /* animated */);
}

static void prv_main(void) {
  prv_handle_init();
  app_event_loop();
}

const PebbleProcessMd* test_mpu_violation_get_info(void) {
  static const PebbleProcessMdSystem s_info = {
    .common.main_func = prv_main,
    // System apps default to privileged; this one must run unprivileged
    // for the MPU to actually evaluate access against the App task regions.
    .common.is_unprivileged = true,
    .name = "MPU violation",
  };
  return (const PebbleProcessMd *)&s_info;
}
