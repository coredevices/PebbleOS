/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum {
  HciStatusCode_Success = 0x00,
  HciStatusCode_UnknownConnectionIdentifier = 0x02,
  HciStatusCode_VS_Base = 0x50,
  HciStatusCode_Max = UINT16_MAX
} HciStatusCode;

#ifndef __clang__
_Static_assert(sizeof(HciStatusCode) == 2, "packed structs expect the status code to be 2 bytes!");
#endif

// disconnect reasons are just status codes
typedef HciStatusCode HciDisconnectReason;
