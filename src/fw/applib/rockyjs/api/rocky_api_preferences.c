/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "rocky_api_preferences.h"

#include "applib/preferred_content_size.h"
#include "rocky_api_util.h"
#include "system/passert.h"

#define ROCKY_USERPREFERENCES "userPreferences"
#define ROCKY_USERPREFERENCES_CONTENTSIZE "contentSize"

static const char *prv_get_content_size_string(void) {
  PreferredContentSize size = preferred_content_size();

  // make sure enum is among known values
  switch (size) {
    case PreferredContentSizeSmall:
    case PreferredContentSizeMedium:
    case PreferredContentSizeLarge:
    case PreferredContentSizeExtraLarge:
      break;
    default:
      size = PreferredContentSizeDefault;
  }

  switch (size) {
    case PreferredContentSizeSmall:
      return "small";
    case PreferredContentSizeMedium:
      return "medium";
    case PreferredContentSizeLarge:
      return "large";
    case PreferredContentSizeExtraLarge:
      return "x-large";
    case NumPreferredContentSizes: {}
  }
  // unreachable
  return "unknown";
}

static jerry_value_t prv_get_content_size(void) {
  return jerry_create_string(
    (const jerry_char_t *)prv_get_content_size_string());
}

static void prv_fill_preferences(jerry_value_t preferences) {
  JS_VAR content_size = prv_get_content_size();

  jerry_set_object_field(preferences, ROCKY_USERPREFERENCES_CONTENTSIZE, content_size);
}

static void prv_init(void) {
  bool was_created = false;
  JS_VAR rocky = rocky_get_rocky_singleton();
  JS_VAR preferences = rocky_get_or_create_object(rocky, ROCKY_USERPREFERENCES,
                                                  rocky_creator_object, NULL, &was_created);
  PBL_ASSERTN(was_created);
  prv_fill_preferences(preferences);
}

const RockyGlobalAPI PREFERENCES_APIS = {
  .init = prv_init,
};
