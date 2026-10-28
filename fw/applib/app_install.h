/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/compiler.h>
#include <pbl/util/uuid.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup AppInstall
//!   \brief Read the installed apps and launch one of them.
//!
//!   The list matches the system menus: hidden entries are left out, and
//!   built-in watchfaces are included. Launching an app does not change the
//!   default watchface. Selecting a watchface stores it as the default and
//!   then launches it. Every call requires the AppInstall grant from the
//!   phone. Without it the call returns @ref AppInstallErrPermissionDenied
//!   and does not read or change installs.
//!   @{

//! Bytes available for an installed app's name, including the terminating NUL.
#define APP_INSTALL_NAME_SIZE 96

typedef enum AppInstallResult {
  AppInstallOk = 0,
  AppInstallErrPermissionDenied,
  AppInstallErrFailed,
} AppInstallResult;

//! One installed app, copied out to the caller.
typedef struct PBL_PACKED {
  //! UUID of the app.
  Uuid uuid;
  //! Display name. Always NUL-terminated.
  char name[APP_INSTALL_NAME_SIZE];
  //! True when the app is a watchface.
  bool is_watchface;
} AppInstallInfo;

//! Number of installed apps visible in the system menus.
//! @param count Set to how many entries @ref app_install_get_at_index can read.
//! @return @ref AppInstallErrFailed if @a count is null.
AppInstallResult app_install_count(uint32_t *count);

//! Copy the installed app at @a index into @a info.
//!
//! Indices run from zero to one less than @ref app_install_count, in the
//! order the system enumerates installs. Hidden entries are not counted.
//! A failed call leaves @a info unchanged.
//! @param index Index of the app to copy.
//! @param info Caller-owned record to fill.
//! @return @ref AppInstallErrFailed if @a index is out of range or @a info is null.
AppInstallResult app_install_get_at_index(uint32_t index, AppInstallInfo *info);

//! Launch an installed app without changing the default watchface.
//!
//! The launch is queued. A watchface launched this way is replaced by the
//! previous default once a later app closes.
//! @param uuid UUID of the installed app.
//! @return @ref AppInstallErrFailed if @a uuid is null or not installed.
AppInstallResult app_launch(const Uuid *uuid);

//! Store an installed watchface as the default and launch it.
//!
//! Refuses a UUID that is missing, not a watchface, or hidden from the
//! system menus. The preference is written before the launch, so closing a
//! later app returns to this face. The launch replaces the caller: schedule
//! any follow-up work, such as the next wakeup, before calling this.
//! @param uuid UUID of the watchface.
//! @return @ref AppInstallErrFailed if @a uuid is not an installed, visible watchface.
AppInstallResult watchface_select(const Uuid *uuid);

//!   @} // group AppInstall
//! @} // group Foundation
