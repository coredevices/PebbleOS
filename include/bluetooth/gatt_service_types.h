/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <util/uuid.h>

#include <stddef.h>
#include <stdint.h>

#include <util/attributes.h>

//! Below are the data structures to store information about a *remote* GATT
//! service and its characteristics and descriptors.
//!
//! It's designed for compactness and ease of serialization, at the cost of
//! CPU cycles to iterate over and access the data.
//! The GATTCharacteristic are tacked at the end of the struct. At the end of
//! each GATTCharacteristic, its descriptors are tacked on. Lastly, after all
//! the characteristics, an array of Included Service handles is tacked on.
//! Struct packing is not enabled at the moment, but could be if needed.
//! Handles for the Characteristics and Descriptors are stored as offsets from
//! the parent service handle to save one byte per characteristic.
//!
//! Ideas for more memory footprint optimizations:
//! - Create a shared list of UUIDs that can be referenced,
//! to avoid wasting 16 bytes of RAM per service, characteristic and descriptor?

typedef struct PACKED ATTHandleRange {
  uint16_t start;
  uint16_t end;
} ATTHandleRange;

//! Common header for GATTDescriptor, GATTCharacteristic and GATTService
typedef struct {
  Uuid uuid;
} GATTObjectHeader;

typedef struct {
  //! The UUID of the descriptor
  Uuid uuid;

  //! The offset of the handle with respect to service.att_handle
  uint8_t att_handle_offset;
} GATTDescriptor;

_Static_assert(offsetof(GATTDescriptor, uuid) == offsetof(GATTObjectHeader, uuid), "");

typedef struct {
  //! The UUID of the characteristic
  Uuid uuid;

  //! The offset of the handle with respect to service.att_handle
  uint8_t att_handle_offset;

  uint8_t properties;

  uint8_t num_descriptors;
  GATTDescriptor descriptors[];
} GATTCharacteristic;

_Static_assert(offsetof(GATTCharacteristic, uuid) == offsetof(GATTObjectHeader, uuid), "");

typedef struct GATTService {
  //! The UUID of the service
  Uuid uuid;

  uint8_t discovery_generation;

  //! The size in bytes of the GATTService blob, including all its
  //! characteristics, descriptors and included service handles.
  uint16_t size_bytes;

  //! The ATT handle of the service
  uint16_t att_handle;

  //! Number of characteristics in the array
  //! @note because GATTCharacteristic is variable length, it is not possible
  //! to use array subscripting.
  uint8_t num_characteristics;

  //! The total number of descriptors in the service
  uint8_t num_descriptors;

  //! Size of the att_handles_included_services array
  uint8_t num_att_handles_included_services;

  //! Array with the characteristics of the service
  GATTCharacteristic characteristics[];

  //! Array with the ATT handles of Included Services
  //! This array follows after the characteristics, when
  //! num_att_handles_included_services > 0
  //! uint16_t att_handles_included_services[];
} GATTService;

_Static_assert(offsetof(GATTService, uuid) == offsetof(GATTObjectHeader, uuid), "");

#define COMPUTE_GATTSERVICE_SIZE_BYTES(num_chars, num_descs, num_includes) \
  (sizeof(GATTService) + sizeof(GATTCharacteristic) * (num_chars) +     \
  sizeof(GATTDescriptor) * (num_descs) + sizeof(uint16_t) * (num_includes))
