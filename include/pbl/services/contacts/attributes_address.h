/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/item.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup services_contacts_attributes_address Contact addresses
 * @ingroup services_contacts
 * @brief (De)serialization of attribute lists together with contact addresses.
 *
 * Deserializing is a two-pass process: parse the payload to size the buffer, then lay out the
 * lists in the buffer and fill them.
 *
 * @code{.c}
 * size_t str_size;
 * uint8_t attrs_per_addr[num_addresses];
 * if (!attributes_address_parse_serial_data(num_attributes, num_addresses, data, size,
 *                                           &str_size, attrs_per_addr)) {
 *   return false;
 * }
 * size_t buf_size = attributes_address_get_buffer_size(num_attributes, num_addresses,
 *                                                      attrs_per_addr, str_size);
 * uint8_t *buf = kernel_malloc_check(buf_size);
 * uint8_t *cursor = buf;
 * attributes_address_init(&attr_list, &addr_list, &cursor, num_attributes, num_addresses,
 *                         attrs_per_addr);
 * bool ok = attributes_address_deserialize(&attr_list, &addr_list, cursor, buf + buf_size,
 *                                          data, size);
 * @endcode
 * @{
 */

/** @brief Address type. */
typedef enum {
  /** Invalid address. */
  AddressTypeInvalid,
  /** Phone number. */
  AddressTypePhoneNumber,
  /** Email address. */
  AddressTypeEmail,
} AddressType;

/** @brief Contact address. */
typedef struct {
  /** Address UUID. */
  Uuid id;
  /** Address type. */
  AddressType type;
  /** Address attributes (the address itself, label, ...). */
  AttributeList attr_list;
} Address;

/** @brief List of addresses. */
typedef struct {
  /** Number of entries in @ref addresses. */
  uint8_t num_addresses;
  /** Addresses. */
  Address *addresses;
} AddressList;

/**
 * @brief Parse serialized attributes and addresses to size the deserialization buffer.
 *
 * @param num_attributes Number of attributes not belonging to an address.
 * @param num_addresses Number of addresses.
 * @param data Serialized attributes followed by serialized addresses.
 * @param data_size Size of @p data in bytes.
 * @param[out] string_alloc_size_out Size needed for all attribute strings.
 * @param[out] attributes_per_address_out Number of attributes of each address, in address
 * order; @p num_addresses entries.
 * @return true if the data was parsed successfully.
 */
bool attributes_address_parse_serial_data(uint8_t num_attributes, uint8_t num_addresses,
                                          const uint8_t *data, size_t data_size,
                                          size_t *string_alloc_size_out,
                                          uint8_t *attributes_per_address_out);

/**
 * @brief Get the buffer size needed to store attributes, addresses and their strings.
 *
 * @param num_attributes Number of attributes not belonging to an address.
 * @param num_addresses Number of addresses.
 * @param attributes_per_address Number of attributes of each address, in address order.
 * @param required_size_for_strings Total size of all attribute strings.
 * @return Required buffer size in bytes.
 */
size_t attributes_address_get_buffer_size(uint8_t num_attributes, uint8_t num_addresses,
                                          const uint8_t *attributes_per_address,
                                          size_t required_size_for_strings);

/**
 * @brief Lay out an attribute list and an address list in a buffer.
 *
 * @param[out] attr_list Attribute list to initialize.
 * @param[out] addr_list Address list to initialize.
 * @param[in,out] buffer Cursor into the buffer; advanced past the attribute and address arrays,
 * leaving it at the space for strings.
 * @param num_attributes Number of attributes not belonging to an address.
 * @param num_addresses Number of addresses.
 * @param attributes_per_address Number of attributes of each address, in address order.
 */
void attributes_address_init(AttributeList *attr_list, AddressList *addr_list, uint8_t **buffer,
                             uint8_t num_attributes, uint8_t num_addresses,
                             const uint8_t *attributes_per_address);

/**
 * @brief Fill an attribute list and an address list from serialized data.
 *
 * The lists must have been set up with attributes_address_init().
 *
 * @param[in,out] attr_list Attribute list to fill.
 * @param[in,out] addr_list Address list to fill.
 * @param buffer Space for strings, as left by attributes_address_init().
 * @param buf_end End of the buffer.
 * @param payload Serialized attributes followed by serialized addresses.
 * @param payload_size Size of @p payload in bytes.
 * @return true on success.
 */
bool attributes_address_deserialize(AttributeList *attr_list, AddressList *addr_list,
                                    uint8_t *buffer, uint8_t *buf_end, const uint8_t *payload,
                                    size_t payload_size);

/**
 * @brief Get the serialized size of an attribute list and an address list.
 *
 * @param list Attribute list.
 * @param addr_list Address list.
 * @return Size in bytes needed by attributes_address_serialize_payload().
 */
size_t attributes_address_get_serialized_payload_size(AttributeList *list, AddressList *addr_list);

/**
 * @brief Serialize an attribute list and an address list.
 *
 * @param attr_list Attribute list.
 * @param addr_list Address list.
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer in bytes.
 * @return Number of bytes written.
 */
size_t attributes_address_serialize_payload(AttributeList *attr_list, AddressList *addr_list,
                                            uint8_t *buffer, size_t buffer_size);

/** @} */
