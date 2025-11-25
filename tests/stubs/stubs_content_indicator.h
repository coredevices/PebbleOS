/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/content_indicator.h"

void content_indicator_init_buffer(ContentIndicatorsBuffer *content_indicators_buffer) {}

ContentIndicator *content_indicator_get_for_scroll_layer(ScrollLayer *scroll_layer) {
  return NULL;
}

ContentIndicator *content_indicator_get_or_create_for_scroll_layer(ScrollLayer *scroll_layer) {
  return NULL;
}

void content_indicator_destroy_for_scroll_layer(ScrollLayer *scroll_layer) {}

void content_indicator_set_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction,
                                             bool available) {}
