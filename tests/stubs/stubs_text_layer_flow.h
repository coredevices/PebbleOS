/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/text_layer.h"

bool text_layer_calc_text_flow_paging_values(const TextLayer *text_layer,
                                             GPoint *content_origin_on_screen,
                                             GRect *page_rect_on_screen) {
  *content_origin_on_screen = GPointZero;
  *page_rect_on_screen = GRectZero;
  return true;
}
