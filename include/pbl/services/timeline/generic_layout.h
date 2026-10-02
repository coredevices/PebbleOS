/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layout_layer.h"
#include "timeline_layout.h"

/**
 * @defgroup services_timeline_generic_layout Generic layout
 * @ingroup services_timeline
 * @brief Layout of generic pins (LayoutIdGeneric).
 * @{
 */

/** @brief Generic pin layout. */
typedef struct {
  /** Base timeline layout. */
  TimelineLayout timeline_layout;
} GenericLayout;

/**
 * @brief Create a generic layout.
 *
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *generic_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of a generic pin.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title is present.
 */
bool generic_layout_verify(bool existing_attributes[]);

/** @} */
