/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>

//! Below are the data structures to store information about a *remote* GATT
//! service and its characteristics and descriptors.
//!
//! It's designed for compactness and ease of serialization, at the cost of
//! CPU cycles to iterate over and access the data.
//! The struct pbl_bt_gatt_characteristic are tacked at the end of the struct. At the end of
//! each struct pbl_bt_gatt_characteristic, its descriptors are tacked on. Lastly, after all
//! the characteristics, an array of Included Service handles is tacked on.
//! Struct packing is not enabled at the moment, but could be if needed.
//! Handles for the Characteristics and Descriptors are stored as offsets from
//! the parent service handle to save one byte per characteristic.
//!
//! Ideas for more memory footprint optimizations:
//! - Create a shared list of UUIDs that can be referenced,
//! to avoid wasting 16 bytes of RAM per service, characteristic and descriptor?

struct PBL_PACKED pbl_bt_att_handle_range {
  uint16_t start;
  uint16_t end;
};

//! Common header for struct pbl_bt_gatt_descriptor, struct pbl_bt_gatt_characteristic and struct
//! pbl_bt_gatt_service
struct pbl_bt_gatt_object_header {
  Uuid uuid;
};

struct pbl_bt_gatt_descriptor {
  //! The UUID of the descriptor
  Uuid uuid;

  //! The offset of the handle with respect to service.att_handle
  uint8_t att_handle_offset;
};

_Static_assert(offsetof(struct pbl_bt_gatt_descriptor, uuid) ==
                   offsetof(struct pbl_bt_gatt_object_header, uuid),
               "");

struct pbl_bt_gatt_characteristic {
  //! The UUID of the characteristic
  Uuid uuid;

  //! The offset of the handle with respect to service.att_handle
  uint8_t att_handle_offset;

  uint8_t properties;

  uint8_t num_descriptors;
  struct pbl_bt_gatt_descriptor descriptors[];
};

_Static_assert(offsetof(struct pbl_bt_gatt_characteristic, uuid) ==
                   offsetof(struct pbl_bt_gatt_object_header, uuid),
               "");

struct pbl_bt_gatt_service {
  //! The UUID of the service
  Uuid uuid;

  uint8_t discovery_generation;

  //! The size in bytes of the struct pbl_bt_gatt_service blob, including all its
  //! characteristics, descriptors and included service handles.
  uint16_t size_bytes;

  //! The ATT handle of the service
  uint16_t att_handle;

  //! Number of characteristics in the array
  //! @note because struct pbl_bt_gatt_characteristic is variable length, it is not possible
  //! to use array subscripting.
  uint8_t num_characteristics;

  //! The total number of descriptors in the service
  uint8_t num_descriptors;

  //! Size of the att_handles_included_services array
  uint8_t num_att_handles_included_services;

  //! Array with the characteristics of the service
  struct pbl_bt_gatt_characteristic characteristics[];

  //! Array with the ATT handles of Included Services
  //! This array follows after the characteristics, when
  //! num_att_handles_included_services > 0
  //! uint16_t att_handles_included_services[];
};

_Static_assert(offsetof(struct pbl_bt_gatt_service, uuid) ==
                   offsetof(struct pbl_bt_gatt_object_header, uuid),
               "");

#define PBL_BT_GATT_SERVICE_SIZE_BYTES(num_chars, num_descs, num_includes)                        \
  (sizeof(struct pbl_bt_gatt_service) + sizeof(struct pbl_bt_gatt_characteristic) * (num_chars) + \
   sizeof(struct pbl_bt_gatt_descriptor) * (num_descs) + sizeof(uint16_t) * (num_includes))
