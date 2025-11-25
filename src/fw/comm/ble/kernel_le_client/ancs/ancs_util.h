/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "ancs_types.h"

#include "services/normal/notifications/notifications.h"

#include <stdbool.h>

int ancs_util_get_notif_attr_response_len(const uint8_t* data, const size_t length);

//! Helper functions that wrap ancs_util_get_attrs, checking if all respective attr list
//! parameters were found
bool ancs_util_is_complete_notif_attr_response(const uint8_t* data, const size_t length, bool* out_error);
bool ancs_util_is_complete_app_attr_dict(const uint8_t* data, const size_t length, bool* out_error);

//! Extract pointers to the start of each attribute in attr_list
//! @param out_error Set if the dictionary was invalid and could not be parsed;
//! if true, bail out!
//! @return True if all requested attributes are present and complete; false if
//! one or more attributes are missing or the last attribute is truncated.
bool ancs_util_get_attr_ptrs(const uint8_t* data, const size_t length, const FetchedAttribute* attr_list,
    const int num_attrs, ANCSAttribute *out_attr_ptrs[], bool* out_error);
