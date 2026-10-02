/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_registry_endpoint Registry endpoint
 * @ingroup services
 * @brief Pebble Protocol registry endpoints.
 * @{
 */

/** @brief Registry endpoint identifiers. */
typedef enum {
  /** System registry. */
  RegistryEndpointIdSystem = 5000,
  /** Factory registry. */
  RegistryEndpointIdFactory = 5001,
} RegistryEndpointId;

/**
 * @brief Handle a message to the system registry endpoint.
 *
 * @param session Session the message was received on.
 * @param data Message payload.
 * @param length_bytes Length of @p data in bytes.
 */
void registry_endpoint_callback(CommSession *session, const uint8_t *data,
                                unsigned int length_bytes);

/**
 * @brief Handle a message to the factory registry endpoint.
 *
 * @param session Session the message was received on.
 * @param data Message payload.
 * @param length_bytes Length of @p data in bytes.
 */
void factory_registry_endpoint_callback(CommSession *session, const uint8_t *data,
                                        unsigned int length_bytes);

/** @} */
