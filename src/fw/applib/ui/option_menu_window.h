/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/ui.h"

#define OPTION_MENU_CHOICE_NONE (-1)

#define OPTION_MENU_STATUS_SEPARATOR_MODE PBL_IF_RECT_ELSE(StatusBarLayerSeparatorModeDotted, \
                                                           StatusBarLayerSeparatorModeNone)

typedef struct OptionMenu OptionMenu;

typedef void (*OptionMenuSelectCallback)(OptionMenu *option_menu, int selection, void *context);
typedef uint16_t (*OptionMenuGetNumRowsCallback)(OptionMenu *option_menu, void *context);
typedef void (*OptionMenuDrawRowCallback)(OptionMenu *option_menu, GContext *ctx,
                                          const Layer *cell_layer, const GRect *text_frame,
                                          uint32_t row, bool selected, void *context);
typedef void (*OptionMenuUnloadCallback)(OptionMenu *option_menu, void *context);
typedef uint16_t (*OptionMenuGetCellHeightCallback)(OptionMenu *option_menu, uint16_t row,
                                                    bool selected, void *context);

typedef uint16_t (*OptionMenuSelectionChangedCallback)(OptionMenu *option_menu, uint16_t row,
                                                    bool selected, void *context);

typedef void (*OptionMenuSelectionWillChangeCallback)(OptionMenu *option_menu,
                                                     uint16_t new_row,
                                                     uint16_t old_row,
                                                     void *context);

                                                    
typedef struct OptionMenuCallbacks {
  OptionMenuSelectCallback select;
  OptionMenuSelectionWillChangeCallback selection_will_change;
  OptionMenuGetNumRowsCallback get_num_rows;
  OptionMenuDrawRowCallback draw_row;
  OptionMenuUnloadCallback unload;
  OptionMenuGetCellHeightCallback get_cell_height;
} OptionMenuCallbacks;

typedef struct OptionMenuColors {
  GColor background;
  GColor foreground;
} OptionMenuColors;

typedef enum OptionMenuContentType {
  //! Content consists of title subtitle or single-line title with ample vertical spacing.
  OptionMenuContentType_Default,
  //! Content consists of a single line.
  OptionMenuContentType_SingleLine,
  //! Content consists of two lines.
  OptionMenuContentType_DoubleLine,

  OptionMenuContentTypeCount
} OptionMenuContentType;

struct OptionMenu {
  Window window;
  StatusBarLayer status_layer;
  MenuLayer menu_layer;
  const char *title;
  GFont title_font;
  OptionMenuContentType content_type;

  GBitmap chosen_image;
  GBitmap not_chosen_image;
  bool icons_enabled;

  OptionMenuCallbacks callbacks;
  void *context;
  int choice;

  OptionMenuColors status_colors;
  OptionMenuColors normal_colors;
  OptionMenuColors highlight_colors;
};

typedef struct {
  const char *title;
  int choice;
  OptionMenuContentType content_type;
  OptionMenuColors status_colors;
  OptionMenuColors highlight_colors;
  bool icons_enabled;
} OptionMenuConfig;

uint16_t option_menu_default_cell_height(OptionMenuContentType content_type, bool selected);

void option_menu_set_status_colors(OptionMenu *option_menu, GColor background, GColor foreground);
void option_menu_set_normal_colors(OptionMenu *option_menu, GColor background, GColor foreground);
void option_menu_set_highlight_colors(OptionMenu *option_menu, GColor background,
                                      GColor foreground);

//! @internal
//! This is currently the only way to set callbacks, which follows (calling it now) 4.x conventions.
//! If option menu must be exported to 3.x, a pass-by value wrapper must be created.
void option_menu_set_callbacks(OptionMenu *option_menu, const OptionMenuCallbacks *callbacks,
                               void *context);

void option_menu_set_title(OptionMenu *option_menu, const char *title);

void option_menu_set_choice(OptionMenu *option_menu, int choice);

void option_menu_set_content_type(OptionMenu *option_menu, OptionMenuContentType content_type);

// enable or disable radio button icons
void option_menu_set_icons_enabled(OptionMenu *option_menu, bool icons_enabled);

void option_menu_reload_data(OptionMenu *option_menu);

//! @internal
//! Use this to set common initialization parameters rather than a group of the particular setters.
void option_menu_configure(OptionMenu *option_menu, const OptionMenuConfig *config);

void option_menu_init(OptionMenu *option_menu);
void option_menu_deinit(OptionMenu *option_menu);

OptionMenu *option_menu_create(void);
void option_menu_destroy(OptionMenu *option_menu);
void option_menu_system_draw_row(OptionMenu *option_menu, GContext *ctx, const Layer *cell_layer,
                                 const GRect *cell_frame, const char *title, bool selected,
                                 void *context);
