/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/ui/layer.h"

#include <stdbool.h>

// Forward declare ScrollLayer to avoid cyclic header include for scroll_layer <-> content_indicator
struct ScrollLayer;
typedef struct ScrollLayer ScrollLayer;

//! @file content_indicator.h
//! @addtogroup UI
//! @{
//!   @addtogroup ContentIndicator
//! \brief Convenience class for rendering arrows to indicate additional content
//!   @{

//! Value to describe directions for \ref ContentIndicator.
//! @see \ref content_indicator_configure_direction
//! @see \ref content_indicator_set_content_available
typedef enum {
  ContentIndicatorDirectionUp = 0, //!< The up direction.
  ContentIndicatorDirectionDown, //!< The down direction.
  NumContentIndicatorDirections //!< The number of supported directions.
} ContentIndicatorDirection;

//! Struct used to configure directions for \ref ContentIndicator.
//! @see \ref content_indicator_configure_direction
typedef struct {
  Layer *layer; //!< The layer where the arrow indicator will be rendered when content is available.
  bool times_out; //!< Whether the display of the arrow indicator should timeout.
  GAlign alignment; //!< The alignment of the arrow within the provided layer.
  struct {
    GColor foreground; //!< The color of the arrow.
    GColor background; //!< The color of the layer behind the arrow.
  } colors;
} ContentIndicatorConfig;

struct ContentIndicator;
typedef struct ContentIndicator ContentIndicator;

//! Creates a ContentIndicator on the heap.
//! @return A pointer to the ContentIndicator. `NULL` if the ContentIndicator could not be created.
ContentIndicator *content_indicator_create(void);

//! Destroys a ContentIndicator previously created using \ref content_indicator_create().
//! @param content_indicator The ContentIndicator to destroy.
void content_indicator_destroy(ContentIndicator *content_indicator);

//! @internal
//! Initializes the given ContentIndicator.
//! @param content_indicator The ContentIndicator to initialize.
void content_indicator_init(ContentIndicator *content_indicator);

//! @internal
//! Deinitializes the given ContentIndicator.
//! @param content_indicator The ContentIndicator to deinitialize.
void content_indicator_deinit(ContentIndicator *content_indicator);

//! @internal
//! Draw an arrow in a rect.
//! @param ctx The graphics context we are drawing in
//! @param frame The rectangle to draw the arrow in
//! @param direction The direction that the arrow points in
//! @param GColor fg_color The fill color of the arrow
//! @param GColor bg_color The fill color of the background
//! @param GAlign alignment The alignment of the arrow within the provided bounds
void content_indicator_draw_arrow(GContext *ctx, const GRect *frame,
                                  ContentIndicatorDirection direction, GColor fg_color,
                                  GColor bg_color, GAlign alignment);

//! Configures a ContentIndicator for the given direction.
//! @param content_indicator The ContentIndicator to configure.
//! @param direction The direction for which to configure the ContentIndicator.
//! @param config The configuration to use to configure the ContentIndicator. If NULL, the data
//! for the specified direction will be reset.
//! @return True if the ContentIndicator was successfully configured for the given direction,
//! false otherwise.
bool content_indicator_configure_direction(ContentIndicator *content_indicator,
                                           ContentIndicatorDirection direction,
                                           const ContentIndicatorConfig *config);

//! Retrieves the availability status of content in the given direction.
//! @param content_indicator The ContentIndicator for which to get the content availability.
//! @param direction The direction for which to get the content availability.
//! @return True if content is available in the given direction, false otherwise.
bool content_indicator_get_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction);

//! Sets the availability status of content in the given direction.
//! @param content_indicator The ContentIndicator for which to set the content availability.
//! @param direction The direction for which to set the content availability.
//! @param available Whether or not content is available.
//! @note If times_out is enabled, calling this function resets any previously scheduled timeout
//! timer for the ContentIndicator.
void content_indicator_set_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction,
                                             bool available);

//!   @} // end addtogroup ContentIndicator
//! @} // end addtogroup UI
