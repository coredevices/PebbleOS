/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layout_layer.h"
#include "timeline_layout.h"

/**
 * @defgroup services_timeline_sports_layout Sports layout
 * @ingroup services_timeline
 * @brief Layout of sports pins (LayoutIdSports).
 * @{
 */

/** @brief State of a game, the value of AttributeIdSportsGameState. */
typedef enum {
  /** Before the game; the card shows the start time. */
  GameStatePreGame = 0,
  /** The game is being played; the card shows the scores. */
  GameStateInGame,
  /** The game is over. */
  GameStatePostGame
} GameState;

/** @brief Sports pin layout. */
typedef struct {
  /** Base timeline layout. */
  TimelineLayout timeline_layout;
  /** Game state, from AttributeIdSportsGameState. */
  GameState state;
} SportsLayout;

/**
 * @brief Create a sports layout.
 *
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *sports_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of a sports pin.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title is present.
 */
bool sports_layout_verify(bool existing_attributes[]);

/** @} */
