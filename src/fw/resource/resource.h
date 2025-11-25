/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "applib/applib_resource.h"
#include "util/attributes.h"

//! @addtogroup Foundation
//! @{
//!   @addtogroup Resources
//!   @{

// TODO: find a cleaner way to do this
typedef uint32_t ResAppNum;

// Needs to be a #define so it can be used in static initializers
#define SYSTEM_APP ((ResAppNum)0)

//! The version information baked into every binary resource pack.
typedef struct PACKED {
  //! The crc of the resource pack between content_start and last_used. See check_bank_crc for how this is calculated.
  uint32_t crc;
  //! Just an identifier, not actually compared to anything.
  uint32_t timestamp;
} ResourceVersion;


//! Types used by pfs_watch_resource()
typedef void (*ResourceChangedCallback)(void *data);
typedef void *ResourceCallbackHandle;


// inits system resources, and sets app resources to an unloaded state
void resource_init(void);

//! @param app_num the resource app to initialize
//! @param version Optional parameter that indicates which version information we should check
//                 against. If NULL is provided no version check is performed.
//! @return True if the resources are valid, false otherwise.
bool resource_init_app(ResAppNum app_num, const ResourceVersion *version);

size_t resource_size(ResAppNum app_num, uint32_t resource_id);

//! Check that a resource id actually exists
bool resource_is_valid(ResAppNum app_num, uint32_t resource_id);

//! @internal
uint32_t resource_get_and_cache(ResAppNum app_num, uint32_t resource_id);

//! @internal
//! @param buffer[out] a buffer to load the data into. Must be at least max_length in bytes.
//! @return Number of bytes actually read. Should be num_bytes for a successful read.
// NOTE: Many things don't properly check the return of this.
size_t resource_load_byte_range_system(ResAppNum app_num, uint32_t resource_id,
                                       uint32_t start_offset, uint8_t *data, size_t num_bytes);

//! @internal
//! Gets a pointer to a data of a built-in resource or memory-addressable resource if possible
const uint8_t *resource_get_readonly_bytes(ResAppNum app_num, uint32_t resource_id,
                                           size_t *num_bytes_out, bool has_privileged_access);

//! @internal
//! True, if given pointer maps to a built-in resource or memory-addressable read-only resource
bool resource_bytes_are_readonly(void *bytes);

//! @internal
//! Retrieve the version of the currently loaded system resources
ResourceVersion resource_get_system_version(void);

//! @internal
//! Retrieve the version of the resource
ResourceVersion resource_get_version(ResAppNum app_num, uint32_t resource_id);

//! @internal
//! Check that two versions are identical
bool resource_version_matches(const ResourceVersion *v1, const ResourceVersion *v2);
//
//! Watch a resource. The callback is called whenever the given resource is modified.
//! NOTE: This currently only supports file-based resources. If the resource is not
//!  file based, then a NULL PFSCallbackHandle will be returned.
ResourceCallbackHandle resource_watch(ResAppNum app_num, uint32_t resource_id,
                                      ResourceChangedCallback callback, void* data);

//! Stop watching a resource.
void resource_unwatch(ResourceCallbackHandle cb_handle);

//!   @} // end addtogroup Resources
//! @} // end addtogroup Foundation
