/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <errno.h>
#include <stdint.h>

//! Status codes. See \ref status_t
typedef enum StatusCode {
  //! Operation completed successfully.
  S_SUCCESS = 0,

  //! An error occurred (no description).
  E_ERROR = -1,

  //! No idea what went wrong.
  E_UNKNOWN = -2,

  //! There was a generic internal logic error.
  E_INTERNAL = -3,

  //! The function was not called correctly.
  E_INVALID_ARGUMENT = -4,

  //! Insufficient allocatable memory available.
  E_OUT_OF_MEMORY = -5,

  //! Insufficient long-term storage available.
  E_OUT_OF_STORAGE = -6,

  //! Insufficient resources available.
  E_OUT_OF_RESOURCES = -7,

  //! Argument out of range (may be dynamic).
  E_RANGE = -8,

  //! Target of operation does not exist.
  E_DOES_NOT_EXIST = -9,

  //! Operation not allowed (may depend on state).
  E_INVALID_OPERATION = -10,

  //! Another operation prevented this one.
  E_BUSY = -11,

  //! Operation not completed; try again.
  E_AGAIN = -12,

  //! Equivalent of boolean true.
  S_TRUE = 1,

  //! Equivalent of boolean false.
  S_FALSE = 0,

  //! For list-style requests.  At end of list.
  S_NO_MORE_ITEMS = 2,

  //! No action was taken as none was required.
  S_NO_ACTION_REQUIRED = 3,

} StatusCode;

//! Return value for system operations. See \ref StatusCode for possible values.
typedef int32_t status_t;

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
