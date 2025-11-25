/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @addtogroup Foundation
//! @{
//!   @addtogroup ExitReason Exit Reason
//!   \brief API for the application to notify the system of the reason it will exit.
//!
//!   If the application has not specified an exit reason before it exits, then the exit reason will
//!   default to APP_EXIT_NOT_SPECIFIED.
//!
//!   Only an application can set its exit reason. The system will not modify it.
//!
//!   @{

//! AppExitReason is used to notify the system of the reason of an application exiting, which may
//! affect the part of the system UI that is presented after the application terminates.
//! @internal
//! New exit reasons may be added in the future. As a best practice, it is recommended to only
//! handle the cases needed, rather than trying to handle all possible exit reasons.
typedef enum AppExitReason {
  APP_EXIT_NOT_SPECIFIED = 0,                    //!< Exit reason not specified
  APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY,        //!< Application performed an action when it exited

  NUM_EXIT_REASONS                               //!< Number of AppExitReason options
} AppExitReason;

//! Returns the current app exit reason.
//! @return The current app exit reason
AppExitReason app_exit_reason_get(void);

//! Set the app exit reason to a new reason.
//! @param reason The new app exit reason
void app_exit_reason_set(AppExitReason exit_reason);

//!   @} // group ExitReason
//! @} // group Foundation
