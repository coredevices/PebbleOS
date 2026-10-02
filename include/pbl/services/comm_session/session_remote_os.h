/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_comm_session_session_remote_os Remote OS
 * @ingroup services_comm_session
 * @brief Phone operating system reported in the platform bitfield.
 * @{
 */

/** @brief Masks within the phone's platform bitfield. */
typedef enum {
  /** Operating system, bits 0 to 2 (a @ref RemoteOS). */
  RemoteBitmaskOS = 0x7,
} RemoteBitmask;

/** @brief Phone operating system. */
typedef enum {
  /** Unknown. */
  RemoteOSUnknown = 0,
  /** iOS. */
  RemoteOSiOS = 1,
  /** Android. */
  RemoteOSAndroid = 2,
  /** macOS. */
  RemoteOSX = 3,
  /** Linux. */
  RemoteOSLinux = 4,
  /** Windows. */
  RemoteOSWindows = 5,
} RemoteOS;

/** @} */
