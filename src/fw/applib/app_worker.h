/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include "util/uuid.h"

//! @addtogroup Foundation
//! @{
//!   @addtogroup AppWorker
//!   \brief Runs in the background, and can communicate with the foreground app.
//!     @{


//! Possible error codes from app_worker_launch, app_worker_kill
typedef enum {
  //! Success
  APP_WORKER_RESULT_SUCCESS= 0,
  //! No worker found for the current app
  APP_WORKER_RESULT_NO_WORKER = 1,
  //! A worker for a different app is already running
  APP_WORKER_RESULT_DIFFERENT_APP = 2,
  //! The worker is not running
  APP_WORKER_RESULT_NOT_RUNNING = 3,
  //! The worker is already running
  APP_WORKER_RESULT_ALREADY_RUNNING = 4,
  //! The user will be asked for confirmation
  APP_WORKER_RESULT_ASKING_CONFIRMATION = 5,
} AppWorkerResult;


//! Generic structure of a worker message that can be sent between an app and its worker
typedef struct {
  uint16_t data0;
  uint16_t data1;
  uint16_t data2;
} AppWorkerMessage;


//! Determine if the worker for the current app is running
//! @return true if running
bool app_worker_is_running(void);

//! Launch the worker for the current app. Note that this is an asynchronous operation, a result code
//! of APP_WORKER_RESULT_SUCCESS merely means that the request was successfully queued up.
//! @return result code
AppWorkerResult app_worker_launch(void);

//! Kill the worker for the current app. Note that this is an asynchronous operation, a result code
//! of APP_WORKER_RESULT_SUCCESS merely means that the request was successfully queued up.
//! @return result code
AppWorkerResult app_worker_kill(void);


//! Callback type for worker messages. Messages can be sent from worker to app or vice versa.
//! @param type An application defined message type
//! @param data pointer to message data. The receiver must know the structure of the data provided by the sender.
typedef void (*AppWorkerMessageHandler)(uint16_t type, AppWorkerMessage *data);

//! Subscribe to worker messages. Once subscribed, the handler gets called on every message emitted by the other task
//! (either worker or app).
//! @param handler A callback to be executed when the event is received
//! @return true on success
bool app_worker_message_subscribe(AppWorkerMessageHandler handler);

//! Unsubscribe from worker messages. Once unsubscribed, the previously registered handler will no longer be called.
//! @return true on success
bool app_worker_message_unsubscribe(void);

//! Send a message to the other task (either worker or app).
//! @param type An application defined message type
//! @param data the message data structure
void app_worker_send_message(uint8_t type, AppWorkerMessage *data);


//!   @} // end addtogroup AppWorker
//! @} // end addtogroup Foundation

//! @internal
//! Register the app message service with the event service system
void app_worker_message_init(void);


