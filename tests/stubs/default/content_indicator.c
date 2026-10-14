/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/content_indicator.h>
#include <applib/ui/content_indicator_private.h>

PBL_WEAK void content_indicator_init_buffer(ContentIndicatorsBuffer *content_indicators_buffer) {
}

PBL_WEAK ContentIndicator *content_indicator_get_for_scroll_layer(ScrollLayer *scroll_layer) {
  return nullptr;
}

PBL_WEAK ContentIndicator *content_indicator_get_or_create_for_scroll_layer(
    ScrollLayer *scroll_layer) {
  return nullptr;
}

PBL_WEAK void content_indicator_destroy_for_scroll_layer(ScrollLayer *scroll_layer) {
}

PBL_WEAK void content_indicator_set_content_available(ContentIndicator *content_indicator,
                                                      ContentIndicatorDirection direction,
                                                      bool available) {
}
