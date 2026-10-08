/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup util_generic_attr Generic attributes
 * @ingroup util
 * @brief Packed lists of id/length/value attributes, as exchanged with the phone.
 * @{
 */

/** @brief Attribute, followed directly by the next one in a list. */
struct PBL_PACKED pbl_generic_attr {
  /** Attribute identifier. */
  uint8_t id;
  /** Length of @ref data in bytes. */
  uint16_t length;
  /** Attribute value. */
  uint8_t data[];
};

/** @brief Attribute list. */
struct PBL_PACKED pbl_generic_attr_list {
  /** Number of attributes in @ref attributes. */
  uint8_t num_attributes;
  /** Attributes, back to back. */
  struct pbl_generic_attr attributes[];
};

/**
 * @brief Find an attribute in a list.
 *
 * Attributes that would extend past @p size end the search.
 *
 * @param attr_list Attribute list.
 * @param id Attribute identifier.
 * @param size Size of the buffer holding @p attr_list, in bytes.
 * @return First attribute with @p id, or NULL if none is found.
 */
struct pbl_generic_attr *pbl_generic_attr_find(struct pbl_generic_attr_list *attr_list, uint8_t id,
                                               size_t size);

/**
 * @brief Write an attribute.
 *
 * The caller must ensure there is room for the header and @p size bytes, and update
 * @ref pbl_generic_attr_list::num_attributes.
 *
 * @param[out] attr Where to write the attribute.
 * @param id Attribute identifier.
 * @param data Attribute value.
 * @param size Length of @p data in bytes.
 * @return Position right after the written attribute, where the next one goes.
 */
struct pbl_generic_attr *pbl_generic_attr_add(struct pbl_generic_attr *attr, uint8_t id,
                                              const void *data, size_t size);

/** @} */
