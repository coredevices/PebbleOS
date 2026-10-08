/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <stdint.h>

/**
 * @defgroup services_timeline_attribute_private Serialized attributes
 * @ingroup services_timeline
 * @brief Wire format of a timeline attribute.
 * @{
 */

/** @brief Header of a serialized attribute, followed by @ref length bytes of value. */
typedef struct PBL_PACKED {
  /** AttributeId. */
  uint8_t id;
  /** Length of the value in bytes, little endian. */
  uint16_t length;
} SerializedAttributeHeader;

/** @} */
