/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/ui.h"
#include "applib/graphics/gdraw_command_image.h"
#include "services/common/evented_timer.h"

#define ICON_WIDTH 30
#define ICON_MARGIN 2
#define NAME_BUFFER_SIZE 30
#define TEXT_VERTICAL_OFFSET 15

typedef struct {
  ButtonId first_button;
  bool is_tap;
  bool vibe_on_start;
} TwoClicksArgs;

typedef struct {
  bool enabled;
  TextLayer *name_layer;
  char name[NAME_BUFFER_SIZE];
  GBitmap *action_icon_bitmap;
  // PDC app icon
  Layer *icon_layer;
  GDrawCommandImage *icon_image;
  // Bitmap app icon
  BitmapLayer *icon_bitmap_layer;
  GBitmap *icon_bitmap;
} AppGraphicNode;

typedef struct {
  Window window;

  TextLayer debug_text_layer;

  AppGraphicNode app_up;
  AppGraphicNode app_select;
  AppGraphicNode app_down; 
  ActionBarLayer action_bar;

  const TwoClicksArgs* args;

  EventedTimerID inactive_timer_id; //!< To go back to watchface after inactivity

  AppInstallId app_up_id;
  AppInstallId app_select_id;
  AppInstallId app_down_id;
} TwoClicksAppData;

// uuid: c9594fce-2c48-47fb-a2f2-8aaa04e5daf0
#define TWO_CLICKS_LAUNCHER_UUID_INIT {0xc9, 0x59, 0x4f, 0xce, 0x2c, 0x48, 0x47, 0xfb, \
                                       0xa2, 0xf2, 0x8a, 0xaa, 0x04, 0xe5, 0xda, 0xf0}

const PebbleProcessMd *two_clicks_launcher_get_app_info();