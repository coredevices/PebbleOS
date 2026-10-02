/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layout_layer.h"
#include "timeline_layout.h"

/**
 * @defgroup services_timeline_weather_layout Weather layout
 * @ingroup services_timeline
 * @brief Layout of weather pins (LayoutIdWeather).
 * @{
 */

/** @brief Time shown by a weather pin, the value of AttributeIdDisplayTime. */
typedef enum {
  /** No time. */
  WeatherTimeType_None = 0,
  /** The pin time (default). */
  WeatherTimeType_Pin,
} WeatherTimeType;

/** @brief Kind of weather pin, the value of AttributeIdWeatherPinKind. */
typedef enum {
  /** Regular forecast (default). */
  WeatherPinKind_None = 0,
  /** Sunrise. */
  WeatherPinKind_Sunrise,
  /** Sunset. */
  WeatherPinKind_Sunset,
} WeatherPinKind;

/** @brief Weather pin layout. */
typedef struct {
  /** Base timeline layout. */
  TimelineLayout timeline_layout;
} WeatherLayout;

/**
 * @brief Create a weather layout.
 *
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *weather_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of a weather pin.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title and a location name are present.
 */
bool weather_layout_verify(bool existing_attributes[]);

/** @} */
