/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <errno.h>

#include <system/status_codes.h>

//! Convert 0 or a negative errno to a status code.
static inline status_t status_from_errno(int err) {
  switch (err) {
    case 0:
      return S_SUCCESS;
    case -ENOENT:
      return E_DOES_NOT_EXIST;
    case -EBUSY:
      return E_BUSY;
    case -EMFILE:
      return E_OUT_OF_RESOURCES;
    case -ENOSPC:
      return E_OUT_OF_STORAGE;
    case -ENOMEM:
      return E_OUT_OF_MEMORY;
    case -EINVAL:
    case -ENAMETOOLONG:
      return E_INVALID_ARGUMENT;
    case -ERANGE:
    case -EFBIG:
      return E_RANGE;
    default:
      return E_INTERNAL;
  }
}
