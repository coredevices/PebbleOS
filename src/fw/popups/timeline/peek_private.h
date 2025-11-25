/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "peek.h"

#include "applib/ui/animation.h"
#include "applib/ui/window.h"
#include "services/normal/timeline/timeline_layout.h"

typedef struct PeekLayout {
  TimelineLayoutInfo info;
  TimelineLayout *timeline_layout;
  TimelineItem *item;
} PeekLayout;

typedef struct TimelinePeek {
  Window window;
  Layer layout_layer;
  PeekLayout *peek_layout;
  Animation *animation; //!< Currently running animation
  bool exists; //!< Whether there exists an item to show in peek.
  bool started; //!< Whether the item has started.
  bool enabled; //!< Whether to persistently show or hide the peek.
  bool visible; //!< Whether the peek is visible or not.
  bool first; //!< Whether the item is the first item in Timeline.
  bool removing_concurrent; //!< Whether the removing concurrent animation is occurring.
  bool future_empty; //!< Whether Timeline future is empty.
} TimelinePeek;

#if UNITTEST
TimelinePeek *timeline_peek_get_peek(void);
#endif
