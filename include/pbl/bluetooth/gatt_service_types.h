/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup bluetooth_gatt_service_types GATT service description
 * @ingroup bluetooth
 * @brief Compact description of a discovered remote GATT service.
 *
 * A service is a single variable-length blob, designed for compactness and ease of
 * serialization at the cost of CPU cycles to access it. The struct pbl_bt_gatt_service header is
 * followed by its characteristics, each followed by its descriptors, then by the ATT handles of
 * the included services. Characteristic and descriptor handles are stored as offsets from the
 * service handle.
 *
 * Characteristics have variable length, so they are walked rather than indexed:
 *
 * @code{.c}
 * const struct pbl_bt_gatt_characteristic *chr = service->characteristics;
 *
 * for (uint8_t i = 0; i < service->num_characteristics; i++) {
 *   uint16_t handle = service->att_handle + chr->att_handle_offset;
 *   // ...
 *   chr = (const void *)&chr->descriptors[chr->num_descriptors];
 * }
 * @endcode
 * @{
 */

/** @brief Inclusive range of ATT handles. */
struct PBL_PACKED pbl_bt_att_handle_range {
  /** First handle. */
  uint16_t start;
  /** Last handle. */
  uint16_t end;
};

/** @brief Common header of descriptors, characteristics and services. */
struct pbl_bt_gatt_object_header {
  /** UUID of the object. */
  Uuid uuid;
};

/** @brief A descriptor of a remote characteristic. */
struct pbl_bt_gatt_descriptor {
  /** UUID of the descriptor. */
  Uuid uuid;

  /** Handle of the descriptor, relative to the service's @c att_handle. */
  uint8_t att_handle_offset;
};

_Static_assert(offsetof(struct pbl_bt_gatt_descriptor, uuid) ==
                   offsetof(struct pbl_bt_gatt_object_header, uuid),
               "");

/** @brief A characteristic of a remote service, followed by its descriptors. */
struct pbl_bt_gatt_characteristic {
  /** UUID of the characteristic. */
  Uuid uuid;

  /** Value handle of the characteristic, relative to the service's @c att_handle. */
  uint8_t att_handle_offset;

  /** Properties, a mask of enum pbl_bt_attribute_property. */
  uint8_t properties;

  /** Number of entries in @ref descriptors. */
  uint8_t num_descriptors;
  /** Descriptors of the characteristic. */
  struct pbl_bt_gatt_descriptor descriptors[];
};

_Static_assert(offsetof(struct pbl_bt_gatt_characteristic, uuid) ==
                   offsetof(struct pbl_bt_gatt_object_header, uuid),
               "");

/** @brief A remote service, followed by its characteristics and included service handles. */
struct pbl_bt_gatt_service {
  /** UUID of the service. */
  Uuid uuid;

  /** Discovery run that found the service. Set by the firmware. */
  uint8_t discovery_generation;

  /** Size in bytes of the whole blob, see PBL_BT_GATT_SERVICE_SIZE_BYTES(). */
  uint16_t size_bytes;

  /** ATT handle of the service. */
  uint16_t att_handle;

  /**
   * Number of characteristics.
   *
   * Characteristics have variable length, so @ref characteristics cannot be subscripted.
   */
  uint8_t num_characteristics;

  /** Total number of descriptors of all characteristics. */
  uint8_t num_descriptors;

  /** Number of included service handles. */
  uint8_t num_att_handles_included_services;

  /**
   * Characteristics of the service.
   *
   * When @ref num_att_handles_included_services is not zero, they are followed by
   * @c uint16_t @c att_handles_included_services[] with the ATT handles of the included
   * services.
   */
  struct pbl_bt_gatt_characteristic characteristics[];
};

_Static_assert(offsetof(struct pbl_bt_gatt_service, uuid) ==
                   offsetof(struct pbl_bt_gatt_object_header, uuid),
               "");

/**
 * @brief Size in bytes of a struct pbl_bt_gatt_service blob.
 *
 * @param num_chars Number of characteristics.
 * @param num_descs Total number of descriptors.
 * @param num_includes Number of included services.
 */
#define PBL_BT_GATT_SERVICE_SIZE_BYTES(num_chars, num_descs, num_includes)                        \
  (sizeof(struct pbl_bt_gatt_service) + sizeof(struct pbl_bt_gatt_characteristic) * (num_chars) + \
   sizeof(struct pbl_bt_gatt_descriptor) * (num_descs) + sizeof(uint16_t) * (num_includes))

/** @} */
