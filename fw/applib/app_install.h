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
//!   then launches it.
//!   @{

//! Bytes available for an installed app's name, including the terminating NUL.
#define APP_INSTALL_NAME_SIZE 96

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
//! @return How many entries @ref app_install_get_at_index can read.
uint32_t app_install_count(void);

//! Copy the installed app at @a index into @a info.
//!
//! Indices run from zero to one less than @ref app_install_count, in the
//! order the system enumerates installs. Hidden entries are not counted.
//! @param index Index of the app to copy.
//! @param info Caller-owned record to fill.
//! @return false if @a index is out of range.
bool app_install_get_at_index(uint32_t index, AppInstallInfo *info);

//! Launch an installed app without changing the default watchface.
//!
//! The launch is queued. A watchface launched this way is replaced by the
//! previous default once a later app closes.
//! @param uuid UUID of the installed app.
//! @return false if @a uuid is not installed.
bool app_launch(const Uuid *uuid);

//! Store an installed watchface as the default and launch it.
//!
//! Refuses a UUID that is missing, not a watchface, or hidden from the
//! system menus. The preference is written before the launch, so closing a
//! later app returns to this face. The launch replaces the caller: schedule
//! any follow-up work, such as the next wakeup, before calling this.
//! @param uuid UUID of the watchface.
//! @return false if @a uuid is not an installed, visible watchface.
bool watchface_select(const Uuid *uuid);

//!   @} // group AppInstall
//! @} // group Foundation
