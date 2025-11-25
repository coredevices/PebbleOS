/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup LaunchReason Launch Reason
//!   \brief API for checking what caused the application to launch.
//!
//!   This includes the system, launch by user interaction (User selects the
//!   application from the launcher menu),
//!   launch by the mobile or a mobile companion application,
//!   or launch by a scheduled wakeup event for the specified application.
//!
//!   @{

//! AppLaunchReason is used to inform the application about how it was launched
//! @note New launch reasons may be added in the future. As a best practice, it
//! is recommended to only handle the cases that the app needs to know about,
//! rather than trying to handle all possible launch reasons.
typedef enum {
  APP_LAUNCH_SYSTEM = 0,  //!< App launched by the system
  APP_LAUNCH_USER,        //!< App launched by user selection in launcher menu
  APP_LAUNCH_PHONE,       //!< App launched by mobile or companion app
  APP_LAUNCH_WAKEUP,      //!< App launched by wakeup event
  APP_LAUNCH_WORKER,      //!< App launched by worker calling worker_launch_app()
  APP_LAUNCH_QUICK_LAUNCH, //!< App launched by user using quick launch
  APP_LAUNCH_TIMELINE_ACTION,  //!< App launched by user opening it from a pin
  APP_LAUNCH_SMARTSTRAP,  //!< App launched by a smartstrap
} AppLaunchReason;

//! Provides the method used to launch the current application.
//! @return The method or reason the current application was launched
AppLaunchReason app_launch_reason(void);

//! Get the argument passed to the app when it was launched.
//! @note Currently the only way to pass arguments to apps is by using an openWatchApp action
//! on a pin.
//! @return The argument passed to the app, or 0 if the app wasn't launched from a Launch App action
uint32_t app_launch_get_args(void);

//!   @} // group Launch_Reason
//! @} // group Foundation

