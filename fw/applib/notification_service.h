/* SPDX-FileCopyrightText: 2026 Eyal Leifenberg */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup NotificationService
//!
//! \brief Provides the number of notifications stored on the watch
//!
//! The NotificationService API lets watchapps display an indicator or count for
//! notifications currently available in the system Notifications app. Notification
//! contents are not exposed.
//! @{

//! Callback type for notification count change events
//! @param count The number of notifications currently stored on the watch
typedef void (*NotificationCountHandler)(uint16_t count);

//! Subscribe to notification count changes.
//! @param handler A callback to execute after a notification is added or removed
void notification_service_subscribe(NotificationCountHandler handler);

//! Unsubscribe from notification count changes.
void notification_service_unsubscribe(void);

//! Get the number of notifications currently stored on the watch.
//! @return The notification count
uint16_t notification_service_peek_count(void);

//!     @} // end addtogroup NotificationService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
