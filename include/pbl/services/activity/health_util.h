/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "apps/system/timeline/text_node.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/**
 * @defgroup services_activity_health_util Health formatting helpers
 * @ingroup services_activity
 * @brief Formatting of durations, distances and paces for the health UI.
 *
 * Distances follow the user's distance units preference (miles or kilometers).
 *
 * @code{.c}
 * char buf[HEALTH_WHOLE_AND_DECIMAL_LENGTH];
 *
 * health_util_format_distance(buf, sizeof(buf), distance_m); // "4.2"
 * const char *units = health_util_get_distance_string("mi", "km");
 * @endcode
 * @{
 */

/** @brief Maximum number of text nodes needed in a text node container. */
#define MAX_TEXT_NODES 5

/** @brief Buffer size for a "00.0" formatted value, with 4 extra bytes for translations. */
#define HEALTH_WHOLE_AND_DECIMAL_LENGTH (sizeof("00.0") + 4)

/**
 * @brief Format a duration as hours and minutes, e.g. "12H 59M".
 *
 * Under an hour only minutes are shown ("59M"), whole hours only show hours ("12H"), and 0 is
 * "0H".
 *
 * @param[out] buffer Output string.
 * @param buffer_size Size of @p buffer.
 * @param duration_s Duration, in seconds.
 * @param i18n_owner i18n owner; call i18n_free_all() on it after use.
 * @return Number of characters written, excluding the terminator, snprintf-style.
 */
int health_util_format_hours_and_minutes(char *buffer, size_t buffer_size, int duration_s,
                                         void *i18n_owner);

/**
 * @brief Create a text node with its own buffer and add it to a container.
 *
 * @param buffer_size Size of the text buffer allocated with the node.
 * @param font Font of the node.
 * @param color Color of the node.
 * @param container Container the node is added to, or NULL.
 * @return New text node.
 */
GTextNodeText *health_util_create_text_node(int buffer_size, GFont font, GColor color,
                                            GTextNodeContainer *container);

/**
 * @brief Create a text node showing a string and add it to a container.
 *
 * @param text Text, which must outlive the node.
 * @param font Font of the node.
 * @param color Color of the node.
 * @param container Container the node is added to, or NULL.
 * @return New text node.
 */
GTextNodeText *health_util_create_text_node_with_text(const char *text, GFont font, GColor color,
                                                      GTextNodeContainer *container);

/**
 * @brief Format a duration as hours, minutes and seconds, e.g. "1:15:32".
 *
 * Hours are omitted under an hour ("5:32").
 *
 * @param[out] buffer Output string.
 * @param buffer_size Size of @p buffer.
 * @param duration_s Duration, in seconds.
 * @param leading_zero Whether to pad the first field to two digits.
 * @param i18n_owner i18n owner; call i18n_free_all() on it after use.
 * @return snprintf() result.
 */
int health_util_format_hours_minutes_seconds(char *buffer, size_t buffer_size, int duration_s,
                                             bool leading_zero, void *i18n_owner);

/**
 * @brief Format a duration as minutes and seconds, e.g. "5:32".
 *
 * @param[out] buffer Output string.
 * @param buffer_size Size of @p buffer.
 * @param duration_s Duration, in seconds.
 * @param i18n_owner i18n owner; call i18n_free_all() on it after use.
 * @return snprintf() result.
 */
int health_util_format_minutes_and_seconds(char *buffer, size_t buffer_size, int duration_s,
                                           void *i18n_owner);

/**
 * @brief Add text nodes showing a duration as hours and minutes, e.g. "12h 59min".
 *
 * Under an hour only minutes are shown, whole hours only show hours, and 0 is "0h". Units are
 * vertically aligned to the bottom of the numbers.
 *
 * @param duration_s Duration, in seconds.
 * @param i18n_owner i18n owner; call i18n_free_all() on it after use.
 * @param number_font Font of the numbers.
 * @param units_font Font of the units.
 * @param color Color of all nodes.
 * @param container Container the nodes are added to.
 */
void health_util_duration_to_hours_and_minutes_text_node(int duration_s, void *i18n_owner,
                                                         GFont number_font, GFont units_font,
                                                         GColor color,
                                                         GTextNodeContainer *container);

/**
 * @brief Split a fraction into whole and tenths parts, rounded to the nearest tenth.
 *
 * For example, 5/2 gives 2 and 5.
 *
 * @param numerator Numerator.
 * @param denominator Denominator.
 * @param[out] whole_part Whole part.
 * @param[out] decimal_part Tenths digit.
 */
void health_util_convert_fraction_to_whole_and_decimal_part(int numerator, int denominator,
                                                            int *whole_part, int *decimal_part);

/**
 * @brief Format a fraction with one decimal, e.g. "42.3".
 *
 * @param[out] buffer Output string.
 * @param buffer_size Size of @p buffer.
 * @param numerator Numerator.
 * @param denominator Denominator.
 * @return snprintf() result.
 */
int health_util_format_whole_and_decimal(char *buffer, size_t buffer_size, int numerator,
                                         int denominator);

/**
 * @brief Get the number of meters in the user's distance unit.
 *
 * @return Meters per mile or per kilometer.
 */
int health_util_get_distance_factor(void);

/**
 * @brief Compute a pace in the user's distance unit.
 *
 * @param time_s Time, in seconds.
 * @param distance_meter Distance, in meters.
 * @return Seconds per mile or kilometer, 0 if @p distance_meter is 0.
 */
time_t health_util_get_pace(int time_s, int distance_meter);

/**
 * @brief Pick the units string matching the user's distance unit.
 *
 * @param miles_string String for miles.
 * @param km_string String for kilometers.
 * @return One of the strings.
 */
const char *health_util_get_distance_string(const char *miles_string, const char *km_string);

/**
 * @brief Format a distance in the user's distance unit with one decimal, e.g. "42.3".
 *
 * @param[out] buffer Output string.
 * @param buffer_size Size of @p buffer.
 * @param distance_m Distance, in meters.
 * @return snprintf() result.
 */
int health_util_format_distance(char *buffer, size_t buffer_size, uint32_t distance_m);

/**
 * @brief Convert a distance to the user's distance unit, as whole and tenths parts.
 *
 * @param distance_m Distance, in meters.
 * @param[out] whole_part Whole part.
 * @param[out] decimal_part Tenths digit.
 */
void health_util_convert_distance_to_whole_and_decimal_part(int distance_m, int *whole_part,
                                                            int *decimal_part);

/** @} */
