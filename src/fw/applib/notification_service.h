/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup NotificationService
//! \brief Read-only access to the wearer's notification state.
//!
//! The NotificationService lets an app find out whether the wearer has notifications they have
//! not yet engaged with, which is useful for showing an unread indicator on a watchface. This
//! API is read-only: apps cannot read notification content, nor create, modify, or dismiss
//! notifications.
//!   @{

//! Get the number of notifications the wearer has not yet engaged with.
//!
//! A notification is counted as unread from the moment it arrives until the wearer acts on it:
//! pressing back or select while it is on screen, scrolling onto it, dismissing it, or opening
//! it in the Notifications app. Dismissing counts whether it happens on the watch or on the
//! phone. A notification popup that times out on its own remains unread, as does one that
//! arrives while Quiet Time is suppressing popups.
//!
//! @note Notification storage is cleared when the watch reboots, so this returns 0 after a
//!       restart regardless of what was unread beforehand.
//!
//! @return Count of unread notifications, saturating at 255.
uint8_t notification_service_get_unread_count(void);

//!   @} // group NotificationService
//! @} // group Foundation
