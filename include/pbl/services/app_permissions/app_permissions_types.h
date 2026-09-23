/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @addtogroup Foundation
//! @{
//!   @addtogroup Permissions
//!   @{

//! Permissions an app can declare in its manifest and the user can grant or deny.
typedef enum AppPermission {
  //! Live microphone capture (`capabilities: ["microphone"]`)
  AppPermission_Microphone = 0,
  AppPermissionCount,
} AppPermission;

//! State of a permission for one app.
typedef enum AppPermissionState {
  //! The app does not declare the permission in its manifest.
  AppPermissionStateNotDeclared = 0,
  //! The permission is declared but has not been granted by the user.
  AppPermissionStateDenied = 1,
  //! The permission is granted.
  AppPermissionStateGranted = 2,
} AppPermissionState;

//!   @} // end addtogroup Permissions
//! @} // end addtogroup Foundation
