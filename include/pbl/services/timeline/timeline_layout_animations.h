/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "timeline_layout.h"

/**
 * @defgroup services_timeline_timeline_layout_animations Timeline layout animations
 * @ingroup services_timeline
 * @brief Icon transitions between the pin and card views of a timeline item.
 * @{
 */

/**
 * @brief Animate the icon from a pin layout to a card layout.
 *
 * Runs for @ref TIMELINE_CARD_TRANSITION_MS. Does nothing if the pin icon cannot be loaded.
 *
 * @param pin_timeline_layout Layout of the pin.
 * @param card_timeline_layout Layout of the card.
 */
void timeline_layout_transition_pin_to_card(TimelineLayout *pin_timeline_layout,
                                            TimelineLayout *card_timeline_layout);

/**
 * @brief Animate the icon from a card layout back to a pin layout.
 *
 * The pin icon is hidden until the animation stops. Does nothing if the card icon cannot be
 * loaded.
 *
 * @param card_timeline_layout Layout of the card.
 * @param pin_timeline_layout Layout of the pin.
 */
void timeline_layout_transition_card_to_pin(TimelineLayout *card_timeline_layout,
                                            TimelineLayout *pin_timeline_layout);

/**
 * @brief Create an animation moving a layout and its icon between two frames.
 *
 * @param layout Layout to move.
 * @param from Start frame of the layout.
 * @param to End frame of the layout.
 * @param icon_from Start frame of the icon.
 * @param icon_to End frame of the icon.
 * @param duration Duration in milliseconds.
 * @param interpolate Interpolation function.
 * @return Animation.
 */
Animation *timeline_layout_create_up_down_animation(TimelineLayout *layout, const GRect *from,
                                                    const GRect *to, const GRect *icon_from,
                                                    const GRect *icon_to, uint32_t duration,
                                                    InterpolateInt64Function interpolate);

/** @} */
