/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/ui/content_indicator.h"
#include "applib/ui/content_indicator_private.h"
#include "util/buffer.h"

// Fakes
////////////////////////////////////

#include "fake_app_timer.h"
#include "fake_content_indicator.h"

// Stubs
////////////////////////////////////

#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_gpath.h"
#include "stubs_graphics.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_ui_window.h"
#include "stubs_unobstructed_area.h"

extern void prv_content_indicator_update_proc(Layer *layer, GContext *ctx);

// Helpers
////////////////////////////////////

static Layer s_content_indicator_dummy_layer;
static LayerUpdateProc s_content_indicator_dummy_layer_update_proc;

ContentIndicatorConfig helper_get_dummy_config(void) {
  return (ContentIndicatorConfig) {
    .layer = &s_content_indicator_dummy_layer,
    .times_out = false,
    .alignment = GAlignLeft,
    .colors = {
      .foreground = GColorGreen,
      .background = GColorRed
    }
  };
}

void helper_check_buffer_for_content_indicator(size_t index, ContentIndicator *content_indicator) {
  ContentIndicatorsBuffer *content_indicators_buffer = content_indicator_get_current_buffer();
  Buffer *buffer = &content_indicators_buffer->buffer;
  ContentIndicator **content_indicators = (ContentIndicator**)buffer->data;
  cl_assert_equal_p(content_indicators[index], content_indicator);
}

void helper_check_configs_for_equality(ContentIndicatorConfig a, ContentIndicatorConfig b) {
  cl_assert_equal_p(a.layer, b.layer);
  cl_assert_equal_b(a.times_out, b.times_out);
  cl_assert(a.alignment == b.alignment);
  cl_assert(a.colors.foreground.argb == b.colors.foreground.argb);
  cl_assert(a.colors.background.argb == b.colors.background.argb);
}

// Setup
/////////////////////////////////

void test_content_indicator__initialize(void) {
  // Initialize the static buffer of content indicators
  content_indicator_init_buffer(content_indicator_get_current_buffer());
  // Reset the dummy layer's fields
  memset(&s_content_indicator_dummy_layer, 0, sizeof(Layer));
}

void test_content_indicator__cleanup(void) {
}

// Tests
////////////////////////////////////

void test_content_indicator__create_should_add_to_buffer(void) {
  ContentIndicator *content_indicator;
  for (size_t i = 0; i < CONTENT_INDICATOR_BUFFER_SIZE; i++) {
    content_indicator = content_indicator_create();
    cl_assert(content_indicator);
    helper_check_buffer_for_content_indicator(i, content_indicator);
  }

  // Creating more content indicators than the buffer can hold should return NULL
  cl_assert_equal_p(content_indicator_create(), NULL);
}

void test_content_indicator__init_should_add_to_buffer(void) {
  ContentIndicator content_indicator;
  for (size_t i = 0; i < CONTENT_INDICATOR_BUFFER_SIZE; i++) {
    content_indicator_init(&content_indicator);
    helper_check_buffer_for_content_indicator(i, &content_indicator);
  }

  // Initializing more content indicators than the buffer can hold should assert
  cl_assert_passert(content_indicator_init(&content_indicator));
}

void test_content_indicator__deinit_should_remove_from_buffer(void) {
  ContentIndicatorsBuffer *content_indicators_buffer = content_indicator_get_current_buffer();
  Buffer *buffer = &content_indicators_buffer->buffer;

  ContentIndicator content_indicator;
  size_t bytes_written = 0;
  for (size_t i = 0; i < CONTENT_INDICATOR_BUFFER_SIZE; i++) {
    content_indicator_init(&content_indicator);
    bytes_written += sizeof(ContentIndicator *);
    cl_assert_equal_i(buffer->bytes_written, bytes_written);
  }

  for (size_t i = 0; i < CONTENT_INDICATOR_BUFFER_SIZE; i++) {
    content_indicator_deinit(&content_indicator);
    bytes_written -= sizeof(ContentIndicator *);
    cl_assert_equal_i(buffer->bytes_written, bytes_written);
  }
}

void test_content_indicator__configuring_should_configure(void) {
  ContentIndicator content_indicator;
  content_indicator_init(&content_indicator);
  ContentIndicatorDirectionData *direction_data = content_indicator.direction_data;

  // Test setting a dummy configuration for a direction
  const ContentIndicatorConfig dummy_config = helper_get_dummy_config();
  const ContentIndicatorDirection direction = ContentIndicatorDirectionUp;
  dummy_config.layer->update_proc = s_content_indicator_dummy_layer_update_proc;
  cl_assert(content_indicator_configure_direction(&content_indicator, direction, &dummy_config));
  helper_check_configs_for_equality(dummy_config, direction_data[direction].config);
  // Should save a reference to the config layer's update proc
  cl_assert_equal_p(dummy_config.layer->update_proc, direction_data->original_update_proc);
  cl_assert_equal_p(direction_data->original_update_proc,
                    s_content_indicator_dummy_layer_update_proc);
}

void test_content_indicator__configuring_different_directions_with_same_layer_should_fail(void) {
  ContentIndicator content_indicator;
  content_indicator_init(&content_indicator);

  // Setting a dummy configuration for a direction should return true
  const ContentIndicatorConfig dummy_config = helper_get_dummy_config();
  dummy_config.layer->update_proc = s_content_indicator_dummy_layer_update_proc;
  cl_assert(content_indicator_configure_direction(&content_indicator,
                                                  ContentIndicatorDirectionUp,
                                                  &dummy_config));

  // Using the same dummy configuration (which has the same layer) to configure a different
  // direction should fail
  cl_assert(!content_indicator_configure_direction(&content_indicator,
                                                   ContentIndicatorDirectionDown,
                                                   &dummy_config));
}

void test_content_indicator__setting_content_available_should_update_layer_update_proc(void) {
  ContentIndicator content_indicator;
  content_indicator_init(&content_indicator);
  ContentIndicatorDirectionData *direction_data = content_indicator.direction_data;

  const ContentIndicatorConfig dummy_config = helper_get_dummy_config();
  const ContentIndicatorDirection direction = ContentIndicatorDirectionUp;
  dummy_config.layer->update_proc = s_content_indicator_dummy_layer_update_proc;
  cl_assert(content_indicator_configure_direction(&content_indicator, direction, &dummy_config));
  cl_assert_equal_p(dummy_config.layer->update_proc, direction_data->original_update_proc);
  cl_assert_equal_p(direction_data->original_update_proc,
                    s_content_indicator_dummy_layer_update_proc);

  // Setting content available should switch the layer's update proc to draw an arrow
  content_indicator_set_content_available(&content_indicator, direction, true);
  cl_assert_equal_p(dummy_config.layer->update_proc, prv_content_indicator_update_proc);

  // Setting content unavailable should revert the layer's update proc
  content_indicator_set_content_available(&content_indicator, direction, false);
  cl_assert_equal_p(dummy_config.layer->update_proc, direction_data->original_update_proc);
  cl_assert_equal_p(direction_data->original_update_proc,
                    s_content_indicator_dummy_layer_update_proc);
}

void test_content_indicator__creating_for_scroll_layer(void) {
  ScrollLayer scroll_layer;
  ContentIndicator *content_indicator = content_indicator_get_or_create_for_scroll_layer(
    &scroll_layer);
  cl_assert(content_indicator);
  // Should save a reference to the scroll layer
  cl_assert_equal_p(content_indicator->scroll_layer, &scroll_layer);

  // Should retrieve the same content indicator with the same scroll layer
  ContentIndicator *content_indicator2 = content_indicator_get_or_create_for_scroll_layer(
    &scroll_layer);
  cl_assert(content_indicator2);
  // Should save a reference to the scroll layer
  cl_assert_equal_p(content_indicator2->scroll_layer, &scroll_layer);
  cl_assert_equal_p(content_indicator2, content_indicator);

  // Should retrieve a different content indicator for a different scroll layer
  ScrollLayer scroll_layer2;
  ContentIndicator *content_indicator3 = content_indicator_get_or_create_for_scroll_layer(
    &scroll_layer2);
  cl_assert(content_indicator3);
  // Should save a reference to the scroll layer
  cl_assert_equal_p(content_indicator3->scroll_layer, &scroll_layer2);
  cl_assert(content_indicator3 != content_indicator);
}

void test_content_indicator__should_only_be_created_for_scroll_layer_upon_client_access(void) {
  ContentIndicatorsBuffer *content_indicators_buffer = content_indicator_get_current_buffer();
  Buffer *buffer = &content_indicators_buffer->buffer;
  // At the start of the test, the buffer should be empty
  cl_assert(buffer_is_empty(buffer));

  ScrollLayer scroll_layer;
  // Trying to access the ContentIndicator for this ScrollLayer should return NULL because we
  // haven't tried to access it as the client yet
  cl_assert_equal_p(content_indicator_get_for_scroll_layer(&scroll_layer), NULL);
  // And the buffer should still be empty
  cl_assert(buffer_is_empty(buffer));

  // Now we try to access it as the client, which should actually create the ContentIndicator
  ContentIndicator *content_indicator = content_indicator_get_or_create_for_scroll_layer(
    &scroll_layer);
  cl_assert(content_indicator);
  // The ContentIndicator should have a reference to the ScrollLayer
  cl_assert_equal_p(content_indicator->scroll_layer, &scroll_layer);
  // The buffer should now hold the newly created ContentIndicator
  cl_assert(buffer->bytes_written == sizeof(ContentIndicator *));

  // Finally, calling content_indicator_get_for_scroll_layer() again should return the same
  // ContentIndicator
  ContentIndicator *content_indicator2 = content_indicator_get_for_scroll_layer(&scroll_layer);
  cl_assert(content_indicator2);
  cl_assert_equal_p(content_indicator2, content_indicator);
  // The buffer should still only hold the single ContentIndicator
  cl_assert(buffer->bytes_written == sizeof(ContentIndicator *));
}

void test_content_indicator__pass_null_config_to_reset_direction_data(void) {
  ContentIndicator content_indicator;
  content_indicator_init(&content_indicator);
  ContentIndicatorDirectionData *direction_data = content_indicator.direction_data;

  const ContentIndicatorConfig dummy_config = helper_get_dummy_config();
  const ContentIndicatorDirection direction = ContentIndicatorDirectionUp;
  dummy_config.layer->update_proc = s_content_indicator_dummy_layer_update_proc;

  cl_assert(content_indicator_configure_direction(&content_indicator, direction, &dummy_config));
  cl_assert_equal_p(dummy_config.layer->update_proc, direction_data->original_update_proc);
  cl_assert_equal_p(direction_data->original_update_proc,
                    s_content_indicator_dummy_layer_update_proc);

  // Setting content available should switch the layer's update proc
  content_indicator_set_content_available(&content_indicator, direction, true);
  cl_assert_equal_p(dummy_config.layer->update_proc, prv_content_indicator_update_proc);

  // Direction data should be emptied and layer's update proc should return to original when NULL
  // config is passed
  cl_assert(content_indicator_configure_direction(&content_indicator, direction, NULL));
  cl_assert_equal_p(dummy_config.layer->update_proc, s_content_indicator_dummy_layer_update_proc);
  cl_assert(!direction_data[direction].config.layer);

  // Setting content available should not change layer update proc without reconfiguring.
  content_indicator_set_content_available(&content_indicator, direction, true);
  cl_assert_equal_p(dummy_config.layer->update_proc, s_content_indicator_dummy_layer_update_proc);
}

void test_content_indicator__re_configure_direction(void) {
  ContentIndicator content_indicator;
  content_indicator_init(&content_indicator);

  const ContentIndicatorConfig dummy_config = helper_get_dummy_config();
  const ContentIndicatorDirection up = ContentIndicatorDirectionUp;
  const ContentIndicatorDirection down = ContentIndicatorDirectionDown;

  cl_assert(content_indicator_configure_direction(&content_indicator, up, &dummy_config));

  // re-configure with the same direction, should be a success
  cl_assert(content_indicator_configure_direction(&content_indicator, up, &dummy_config));

  // re-configure with a different same direction, should fail
  cl_assert(!content_indicator_configure_direction(&content_indicator, down, &dummy_config));
}
