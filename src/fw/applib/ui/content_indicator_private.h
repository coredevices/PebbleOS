/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_timer.h"
#include "applib/ui/scroll_layer.h"
#include "applib/ui/layer.h"
#include "util/buffer.h"

typedef struct {
  ContentIndicatorDirection direction:2;
  bool content_available:1;
  AppTimer *timeout_timer;
  ContentIndicatorConfig config;
  LayerUpdateProc original_update_proc;
} ContentIndicatorDirectionData;

struct ContentIndicator {
  ContentIndicatorDirectionData direction_data[NumContentIndicatorDirections];
  //! Needed to find the ContentIndicator belonging to a ScrollLayer
  //! @see \ref content_indicator_get_or_create_for_scroll_layer()
  ScrollLayer *scroll_layer;
};

//! TODO: There are no videos from design yet for this timeout, so it was arbitrarily chosen.
#define CONTENT_INDICATOR_TIMEOUT_MS 1200

//! The maximum number of ContentIndicator pointers that a ContentIndicatorsBuffer should hold.
//! This affects two separate buffers: one for kernel (i.e. all modals together) and one for the
//! currently running app. If an attempt is made to exceed this size by initializing an additional
//! ContentIndicator, then \ref content_indicator_init() will trigger an assertion. If an attempt
//! is made to exceed this size by creating an additional ContentIndicator, then
//! \ref content_indicator_create() will return `NULL`.
#define CONTENT_INDICATOR_BUFFER_SIZE 4

//! The maximum size (in Bytes) of the buffer of ContentIndicators.
#define CONTENT_INDICATOR_BUFFER_SIZE_BYTES (CONTENT_INDICATOR_BUFFER_SIZE * \
                                             sizeof(ContentIndicator *))

//! This union allows us to statically allocate the storage for a buffer of content indicators.
typedef union {
  Buffer buffer;
  uint8_t buffer_storage[sizeof(Buffer) + CONTENT_INDICATOR_BUFFER_SIZE_BYTES];
} ContentIndicatorsBuffer;

//! @internal
//! Retrieves the ContentIndicator for the given ScrollLayer.
//! @param scroll_layer The ScrollLayer for which to retrieve the ContentIndicator.
//! @return A pointer to the ContentIndicator.
//! `NULL` if the ContentIndicator could not be found.
ContentIndicator *content_indicator_get_for_scroll_layer(ScrollLayer *scroll_layer);

//! @internal
//! Retrieves the ContentIndicator for the given ScrollLayer, or creates one if none exists.
//! @param scroll_layer The ScrollLayer for which to retrieve the ContentIndicator.
//! @return A pointer to the ContentIndicator.
//! `NULL` if the ContentIndicator could not be found and could not be created.
ContentIndicator *content_indicator_get_or_create_for_scroll_layer(ScrollLayer *scroll_layer);

//! @internal
//! Destroys a ContentIndicator for the given ScrollLayer.
//! @param scroll_layer The ScrollLayer for which to destroy a ContentIndicator.
void content_indicator_destroy_for_scroll_layer(ScrollLayer *scroll_layer);

//! @internal
//! Initializes the given ContentIndicatorsBuffer.
//! @param content_indicators_buffer A pointer to the ContentIndicatorsBuffer to initialize.
void content_indicator_init_buffer(ContentIndicatorsBuffer *content_indicators_buffer);
