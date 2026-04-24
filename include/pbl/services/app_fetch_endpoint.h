/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/uuid.h"
#include "kernel/events.h"
#include "process_management/app_install_types.h"

typedef enum {
  AppFetchResultSuccess,
  AppFetchResultTimeoutError,
  AppFetchResultGeneralFailure,
  AppFetchResultPhoneBusy,
  AppFetchResultUUIDInvalid,
  AppFetchResultNoBluetooth,
  AppFetchResultPutBytesFailure,
  AppFetchResultNoData,
  AppFetchResultUserCancelled,
  AppFetchResultIncompatibleJSFailure,
} AppFetchResult;

typedef struct {
  AppFetchResult error;
  AppInstallId id;
} AppFetchError;

void app_fetch_binaries(const Uuid *uuid, AppInstallId app_id, bool has_worker);

//! @param app_id The AppInstallId of the fetch to be cancelled.
//! NOTE: If `app_id` is INSTALL_ID_INVALID, it will cancel the fetch regardless of AppInstallId
void app_fetch_cancel(AppInstallId app_id);

//! @param app_id The AppInstallId of the fetch to be cancelled.
//! NOTE: If `app_id` is INSTALL_ID_INVALID, it will cancel the fetch regardless of AppInstallId
//! NOTE: Must be called from PebbleTask_KernelBackground
void app_fetch_cancel_from_system_task(AppInstallId app_id);

bool app_fetch_in_progress(void);

//! Put Bytes handler. Used for keeping track of progress and cleanup events
void app_fetch_put_bytes_event_handler(PebblePutBytesEvent *pb_event);

AppFetchError app_fetch_get_previous_error(void);
