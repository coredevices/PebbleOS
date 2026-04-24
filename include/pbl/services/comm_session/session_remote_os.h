/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum {
  RemoteBitmaskOS = 0x7, // bits 0 - 2
} RemoteBitmask;

typedef enum {
  RemoteOSUnknown = 0,
  RemoteOSiOS = 1,
  RemoteOSAndroid = 2,
  RemoteOSX = 3,
  RemoteOSLinux = 4,
  RemoteOSWindows = 5,
} RemoteOS;
