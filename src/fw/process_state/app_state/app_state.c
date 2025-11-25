/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_state/app_state/app_state.h"

#include "applib/app_message/app_message_internal.h"
#include "applib/event_service_client.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/pbl_std/locale.h"
#include "applib/ui/animation_private.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/layer.h"
#include "applib/ui/recognizer/recognizer_list.h"
#include "applib/unobstructed_area_service.h"
#include "kernel/util/segment.h"
#include "process_management/process_loader.h"
#include "process_management/process_manager.h"
#include "services/common/i18n/i18n.h"
#include "services/normal/persist.h"
#include "syscall/syscall_internal.h"
#include "system/passert.h"
#include "util/attributes.h"
#include "tinymt32.h"

#include <string.h>

typedef struct {
  Heap heap;

  struct tm gmtime_tm;
  struct tm localtime_tm;
  char localtime_zone[TZ_LEN];

  tinymt32_t rand_seed;

  ProcessAppSDKType sdk_type;
  int16_t initial_obstruction_origin_y;

  ClickManager click_manager;

  AppInbox *app_message_inbox;

  EventServiceInfo app_outbox_subscription_info;

  ApplibInternalEventsInfo applib_internal_events_info;

  AnimationState animation_state;

  AppMessageCtx app_message_ctx;

  WindowStack window_stack;

  FrameBuffer framebuffer;

  GContext graphics_context;

  EventServiceInfo event_service_state;

#if !defined(PLATFORM_TINTIN)
  BLEAppState ble_app_state;
#endif

  AccelServiceState accel_state;

  CompassServiceConfig *compass_config;

  PluginServiceState plugin_service_state;

  void* user_data;

  LogState log_state;

  BatteryStateServiceState battery_state_service_state;

  TickTimerServiceState tick_timer_service_state;

  ConnectionServiceState connection_service_state;

#if CAPABILITY_HAS_HEALTH_TRACKING
  HealthServiceState health_service_state;
#endif

  LocaleInfo locale_info;

#if CAPABILITY_HAS_ACCESSORY_CONNECTOR
  SmartstrapConnectionState smartstrap_state;
#endif

  ContentIndicatorsBuffer content_indicators_buffer;

  bool app_framebuffer_render_pending;

  AppFocusState app_focus_state;

  UnobstructedAreaState unobstructed_area_service_state;

  Layer* layer_tree_stack[LAYER_TREE_STACK_SIZE];

  WakeupHandler wakeup_handler;

  EventServiceInfo wakeup_event_info;

#if CAPABILITY_HAS_TOUCHSCREEN
  RecognizerList recognizer_list;
#endif

  //! Potentially un-aligned w.r.t. JerryScript's requirements:
  uint8_t *rocky_runtime_context_buffer;
  //! Aligned w.r.t. JerryScript's requirements, pointing into rocky_runtime_context_buffer.
  RockyRuntimeContext *rocky_runtime_context;

  RockyMemoryAPIContext *rocky_memory_api_context;

#if CAPABILITY_HAS_APP_GLANCES
  AppGlance glance;
#endif

  TextRenderState text_render_state;

  bool text_perimeter_debugging_enabled;

  TimelineItemActionSource current_timeline_item_action_source;

  GBitmap *legacy2_framebuffer;
} AppState;

KERNEL_READONLY_DATA static AppState *s_app_state_ptr;

bool app_state_configure(MemorySegment *app_state_ram,
                         ProcessAppSDKType sdk_type,
                         int16_t obstruction_origin_y) {
  s_app_state_ptr = memory_segment_split(app_state_ram, NULL, sizeof(AppState));
  if (!s_app_state_ptr) {
    return false;
  }

  s_app_state_ptr->sdk_type = sdk_type;
  s_app_state_ptr->initial_obstruction_origin_y = obstruction_origin_y;

  if (GBITMAP_NATIVE_FORMAT != GBitmapFormat1Bit &&
      sdk_type == ProcessAppSDKType_Legacy2x) {
    // When running legacy2 aplite apps on basalt we actually have some space
    // after AppState that we don't need, because legacy2 aplite apps need to
    // support running on the smaller platform anyway. We can use this space for
    // doing legacy2 aplite-compatibility work. Note we don't have to worry
    // about 3.x aplite here because we don't support running 3.x aplite apps on
    // 3.x basalt platforms.

    s_app_state_ptr->legacy2_framebuffer = memory_segment_split(
        app_state_ram, NULL, sizeof(GBitmap));
    if (!s_app_state_ptr->legacy2_framebuffer) {
      return false;
    }

    uint16_t row_size = gbitmap_format_get_row_size_bytes(
        LEGACY_2X_DISP_COLS, GBitmapFormat1Bit);
    void *fb_data = memory_segment_split(app_state_ram, NULL,
                                         row_size * LEGACY_2X_DISP_ROWS);
    if (!fb_data) {
      return false;
    }

    *s_app_state_ptr->legacy2_framebuffer = (GBitmap) {
      .addr = fb_data,
      .row_size_bytes = row_size,
      .info.is_bitmap_heap_allocated = false,
      .info.format = GBitmapFormat1Bit,
      .info.version = GBITMAP_VERSION_0,
      .bounds = { { 0, 0 }, { LEGACY_2X_DISP_COLS, LEGACY_2X_DISP_ROWS } }
    };
  }
  return true;
}

NOINLINE void app_state_init(void) {
  s_app_state_ptr->rand_seed.mat1 = 0; // Uninitialized

  click_manager_init(&s_app_state_ptr->click_manager);

  animation_private_state_init(&s_app_state_ptr->animation_state);

  app_message_init();

  app_outbox_init();

  // Set the correct framebuffer size depending on the SDK version
  GSize fb_size;
  app_manager_get_framebuffer_size(&fb_size);
  framebuffer_init(&s_app_state_ptr->framebuffer, &fb_size);

  framebuffer_clear(&s_app_state_ptr->framebuffer);

  const GContextInitializationMode init_mode =
      (s_app_state_ptr->sdk_type == ProcessAppSDKType_System) ? GContextInitializationMode_System :
                                                            GContextInitializationMode_App;
  graphics_context_init(&s_app_state_ptr->graphics_context,
                        &s_app_state_ptr->framebuffer, init_mode);


#if !defined(PLATFORM_TINTIN)
  ble_init_app_state();
#endif

  accel_service_state_init(app_state_get_accel_state());

  plugin_service_state_init(app_state_get_plugin_service());

  battery_state_service_state_init(app_state_get_battery_state_service_state());

  connection_service_state_init(app_state_get_connection_service_state());

  tick_timer_service_state_init(app_state_get_tick_timer_service_state());

#if CAPABILITY_HAS_HEALTH_TRACKING
  health_service_state_init(app_state_get_health_service_state());
#endif

  locale_init_app_locale(app_state_get_locale_info());

  content_indicator_init_buffer(app_state_get_content_indicators_buffer());

  unobstructed_area_service_init(app_state_get_unobstructed_area_state(),
                                 s_app_state_ptr->initial_obstruction_origin_y);

#if CAPABILITY_HAS_APP_GLANCES && !RECOVERY_FW
  app_glance_service_init_glance(app_state_get_glance());
#endif

  s_app_state_ptr->current_timeline_item_action_source = TimelineItemActionSourceModalNotification;
}

NOINLINE void app_state_deinit(void) {
  animation_private_state_deinit(&s_app_state_ptr->animation_state);
#if CAPABILITY_HAS_ACCESSORY_CONNECTOR
  app_smartstrap_cleanup();
#endif
#if CAPABILITY_HAS_HEALTH_TRACKING
  health_service_state_deinit(app_state_get_health_service_state());
#endif
  unobstructed_area_service_deinit(app_state_get_unobstructed_area_state());
}

Heap *app_state_get_heap(void) {
  return &s_app_state_ptr->heap;
}

struct tm *app_state_get_gmtime_tm(void) {
  return &s_app_state_ptr->gmtime_tm;
}
struct tm *app_state_get_localtime_tm(void) {
  return &s_app_state_ptr->localtime_tm;
}
char *app_state_get_localtime_zone(void) {
  return s_app_state_ptr->localtime_zone;
}
void *app_state_get_rand_ptr(void) {
  return &s_app_state_ptr->rand_seed;
}

AppInbox **app_state_get_app_message_inbox(void) {
  return &s_app_state_ptr->app_message_inbox;
}

EventServiceInfo *app_state_get_app_outbox_subscription_info(void) {
  return &s_app_state_ptr->app_outbox_subscription_info;
}

AnimationState* app_state_get_animation_state() {
  return &s_app_state_ptr->animation_state;
}

AppMessageCtx *app_state_get_app_message_ctx(void) {
  return &s_app_state_ptr->app_message_ctx;
}

#if !defined(PLATFORM_TINTIN)
BLEAppState* app_state_get_ble_app_state(void) {
  return &s_app_state_ptr->ble_app_state;
}
#endif

ClickManager* app_state_get_click_manager() {
  return &s_app_state_ptr->click_manager;
}

WindowStack* app_state_get_window_stack() {
  return &s_app_state_ptr->window_stack;
}

FrameBuffer* app_state_get_framebuffer() {
  return &s_app_state_ptr->framebuffer;
}

GContext* app_state_get_graphics_context() {
  return &s_app_state_ptr->graphics_context;
}

EventServiceInfo* app_state_get_event_service_state(void) {
  return &s_app_state_ptr->event_service_state;
}

void app_state_set_user_data(void *data) {
  s_app_state_ptr->user_data = data;
}

void* app_state_get_user_data(void) {
  return s_app_state_ptr->user_data;
}

AccelServiceState* app_state_get_accel_state(void) {
  return &s_app_state_ptr->accel_state;
}

CompassServiceConfig **app_state_get_compass_config(void) {
  return &s_app_state_ptr->compass_config;
}

PluginServiceState *app_state_get_plugin_service(void) {
  return &s_app_state_ptr->plugin_service_state;
}

LogState *app_state_get_log_state(void) {
  return &s_app_state_ptr->log_state;
}

BatteryStateServiceState *app_state_get_battery_state_service_state(void) {
  return &s_app_state_ptr->battery_state_service_state;
}

TickTimerServiceState *app_state_get_tick_timer_service_state(void) {
  return &s_app_state_ptr->tick_timer_service_state;
}

ConnectionServiceState *app_state_get_connection_service_state(void) {
  return &s_app_state_ptr->connection_service_state;
}

#if CAPABILITY_HAS_HEALTH_TRACKING
HealthServiceState *app_state_get_health_service_state(void) {
  return &s_app_state_ptr->health_service_state;
}
#endif

#if CAPABILITY_HAS_ACCESSORY_CONNECTOR
SmartstrapConnectionState *app_state_get_smartstrap_state(void) {
  return &s_app_state_ptr->smartstrap_state;
}
#endif

ContentIndicatorsBuffer *app_state_get_content_indicators_buffer(void) {
  return &s_app_state_ptr->content_indicators_buffer;
}

LocaleInfo *app_state_get_locale_info(void) {
  return &s_app_state_ptr->locale_info;
}

bool *app_state_get_framebuffer_render_pending() {
  return &s_app_state_ptr->app_framebuffer_render_pending;
}

Layer** app_state_get_layer_tree_stack(void) {
  return s_app_state_ptr->layer_tree_stack;
}

AppFocusState *app_state_get_app_focus_state(void) {
  return &s_app_state_ptr->app_focus_state;
}

UnobstructedAreaState *app_state_get_unobstructed_area_state(void) {
  return &s_app_state_ptr->unobstructed_area_service_state;
}

#if CAPABILITY_HAS_APP_GLANCES
AppGlance *app_state_get_glance(void) {
  return &s_app_state_ptr->glance;
}
#endif

WakeupHandler app_state_get_wakeup_handler(void) {
  return s_app_state_ptr->wakeup_handler;
}

void app_state_set_wakeup_handler(WakeupHandler handler) {
  s_app_state_ptr->wakeup_handler = handler;
}

EventServiceInfo *app_state_get_wakeup_event_info(void) {
  return &s_app_state_ptr->wakeup_event_info;
}

GBitmap* app_state_legacy2_get_2bit_framebuffer(void) {
#ifdef PLATFORM_TINTIN
  // Tintin platforms have a native framebuffer type of 2bit, they shouldn't be calling this.
  WTF;
#endif

  PBL_ASSERTN(s_app_state_ptr->legacy2_framebuffer);
  return s_app_state_ptr->legacy2_framebuffer;
}

#if CAPABILITY_HAS_TOUCHSCREEN
RecognizerList *app_state_get_recognizer_list(void) {
  return &s_app_state_ptr->recognizer_list;
}
#endif

RockyRuntimeContext *app_state_get_rocky_runtime_context(void) {
  return s_app_state_ptr->rocky_runtime_context;
}

uint8_t *app_state_get_rocky_runtime_context_buffer(void) {
  return s_app_state_ptr->rocky_runtime_context_buffer;
}

void app_state_set_rocky_runtime_context(uint8_t *unaligned_buffer,
                                         RockyRuntimeContext *rocky_runtime_context) {
  s_app_state_ptr->rocky_runtime_context_buffer = unaligned_buffer;
  s_app_state_ptr->rocky_runtime_context = rocky_runtime_context;
}

RockyMemoryAPIContext *app_state_get_rocky_memory_api_context(void) {
  return s_app_state_ptr->rocky_memory_api_context;
}

void app_state_set_rocky_memory_api_context(RockyMemoryAPIContext *context) {
  s_app_state_ptr->rocky_memory_api_context = context;
}

ApplibInternalEventsInfo *app_state_get_applib_internal_events_info(void) {
  return &s_app_state_ptr->applib_internal_events_info;
}

TextRenderState *app_state_get_text_render_state(void) {
  return &s_app_state_ptr->text_render_state;
}

bool app_state_get_text_perimeter_debugging_enabled(void) {
  return s_app_state_ptr->text_perimeter_debugging_enabled;
}

void app_state_set_text_perimeter_debugging_enabled(bool enabled) {
  s_app_state_ptr->text_perimeter_debugging_enabled = enabled;
}

TimelineItemActionSource app_state_get_current_timeline_item_action_source(void) {
  return s_app_state_ptr->current_timeline_item_action_source;
}

void app_state_set_current_timeline_item_action_source(TimelineItemActionSource current_source) {
  s_app_state_ptr->current_timeline_item_action_source = current_source;
}

// Serial Commands
///////////////////////////////////////////////////////////
#ifdef MALLOC_INSTRUMENTATION
void command_dump_malloc_app(void) {
  heap_dump_malloc_instrumentation_to_dbgserial(app_state_get_heap());
}
#endif
