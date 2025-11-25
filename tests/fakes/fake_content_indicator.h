/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/content_indicator_private.h"

static ContentIndicatorsBuffer s_content_indicators_buffer;

ContentIndicatorsBuffer *content_indicator_get_current_buffer(void) {
  return &s_content_indicators_buffer;
}
