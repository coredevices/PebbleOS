/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! Different types of objects that can be transferred over GetBytes
typedef enum {
  GetBytesObjectUnknown = 0x00,
  GetBytesObjectCoredump = 0x01,
  GetBytesObjectFile = 0x02,
  GetBytesObjectFlash = 0x03
} GetBytesObjectType;

// Possible values for GetBytesRspObjectInfo.error_code
typedef enum {
  GET_BYTES_OK = 0,
  GET_BYTES_MALFORMED_COMMAND = 1,
  GET_BYTES_ALREADY_IN_PROGRESS = 2,
  GET_BYTES_DOESNT_EXIST = 3,
  GET_BYTES_CORRUPTED = 4,
} GetBytesInfoErrorCode;
