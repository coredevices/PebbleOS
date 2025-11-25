# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0



# globally used functions to put other templates in
class FileScope:
    header = """
#pragma once
#include "pebble.h"

void {watch_name}_init(Layer *window);
"""

    includes = """
#include "{header_name}"
#include "resource_ids.gen.h"
#include <time.h>

"""

    init_function = """
void {watch_name}_init(Layer *window) {{
  const GPoint CENTER = {{window->frame.size.w / 2, window->frame.size.h / 2}};

{init_content}

  window->update_proc = &{watch_name}_update;
}}
"""
    update_function = """
static void {watch_name}_update(Layer *me) {{
  const GPoint CENTER = {{me->frame.size.w / 2, me->frame.size.h / 2}};
  (void)me;
  PblTm t;
  get_time(&t);

  {update_content}

}}
"""

# TODO: make the app number an actual function call
    defs_file = """
#pragma once

#define PBL_GEN_LOCAL_INCLUDE #include "{watch_name}.h"

#define PBL_GEN_COMPANY_NAME_STR "{company_name}"
#define PBL_GEN_VISIBLE_NAME_STR "{visible_name}"
#define PBL_GEN_INIT(win_layer) {watch_name}_init(win_layer)

"""

# non-moving image (like a background)
class StaticImage:
    static_defs = """
static BmpContainer s_{name};
"""
    init_lines = """
  bmp_init_container({res_def_name}, &s_{name});
  layer_add_child(window, &s_{name}.layer.layer);
"""

# formatted textual date strings
class TimeText:
    static_defs = """
static TextLayer s_{name};
static char s_{name}_buffer[{buffer_length}] = "";
"""
    # TODO: getting fonts is ghetto-ugly in code right now
    init_lines = """
  text_layer_init(&s_{name}, GRect({x}, {y}, {w}, {h}));
  text_layer_set_text(&s_{name}, s_{name}_buffer);
  text_layer_set_text_color(&s_{name}, {color});
  text_layer_set_background_color(&s_{name}, {background_color});
  GFont {name}_font = {font_func};
  text_layer_set_font(&s_{name}, {name}_font);
  layer_add_child(window, &s_{name}.layer);
"""
    update_lines = """
  string_format_time(s_{name}_buffer, sizeof(s_{name}_buffer), "{date_format_string}", &t);
  text_layer_set_text(&s_{name}, s_{name}_buffer);
"""

# common analog hands
class Hand:
    static_defs = """
static RotBmpPairContainer s_{name};
"""
    init_lines = """
  rotbmp_pair_init_container({res_def_name}_WHITE, {res_def_name}_BLACK, &s_{name});
  rotbmp_pair_layer_set_src_ic(&s_{name}.layer, GPoint({pivot_x}, {pivot_y}));
  GRect {name}_frame = layer_get_frame(&s_{name}.layer.layer);
  {name}_frame.origin.x = CENTER.x - ({name}_frame.size.w / 2);
  {name}_frame.origin.y = CENTER.y - ({name}_frame.size.h / 2);
  layer_set_frame(&s_{name}.layer.layer, {name}_frame);
  layer_add_child(window, &s_{name}.layer.layer);
"""
    update_lines = """
  rotbmp_pair_layer_set_angle(&s_{name}.layer, TRIG_MAX_ANGLE * {angle_ratio});
"""
