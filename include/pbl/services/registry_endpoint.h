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

/** @} */
