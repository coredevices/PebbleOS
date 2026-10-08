/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/uuid.h>

#include <kernel/events.h>
#include <process_management/app_install_types.h>

/**
 * @defgroup services_app_fetch_endpoint App fetch endpoint
 * @ingroup services
 * @brief Fetches app binaries from the phone.
 *
 * The watch sends an install request for an app to the phone, which then pushes the app, worker
 * and resources over put_bytes. Progress, completion and errors are reported with
 * @c PEBBLE_APP_FETCH_EVENT events; once all parts are received the app is added to the app cache.
 * Only one fetch can be in progress at a time. The work runs on KernelBackground.
 * @{
 */

/** @brief Outcome of an app fetch. */
typedef enum {
  /** All parts were received and the app was added to the app cache. */
  AppFetchResultSuccess,
  /** The phone did not start or continue the put_bytes transfer in time. */
  AppFetchResultTimeoutError,
  /** Unexpected message, unexpected put_bytes object or failure to update the app cache. */
  AppFetchResultGeneralFailure,
  /** The phone is busy. */
  AppFetchResultPhoneBusy,
  /** The phone does not know the requested UUID. */
  AppFetchResultUUIDInvalid,
  /** The request could not be sent to the phone. */
  AppFetchResultNoBluetooth,
  /** The put_bytes transfer failed. */
  AppFetchResultPutBytesFailure,
  /** The phone has no data for the app, e.g. no internet connection. */
  AppFetchResultNoData,
  /** The fetch was cancelled. */
  AppFetchResultUserCancelled,
  /** The app's JavaScript is incompatible. */
  AppFetchResultIncompatibleJSFailure,
} AppFetchResult;

/** @brief Result of the last app fetch. */
typedef struct {
  /** Outcome of the fetch. */
  AppFetchResult error;
  /** App that was fetched. */
  AppInstallId id;
} AppFetchError;

/**
 * @brief Request the binaries of an app from the phone.
 *
 * Ignored if a fetch is already in progress.
 *
 * @param uuid UUID of the app.
 * @param app_id Install id of the app.
 * @param has_worker true if the app has a worker binary to fetch.
 */
void app_fetch_binaries(const Uuid *uuid, AppInstallId app_id, bool has_worker);

/**
 * @brief Cancel a fetch in progress.
 *
 * The cancellation runs asynchronously on KernelBackground.
 *
 * @param app_id App whose fetch is cancelled; @c INSTALL_ID_INVALID cancels any fetch.
 */
void app_fetch_cancel(AppInstallId app_id);

/**
 * @brief Cancel a fetch in progress, synchronously.
 *
 * Must be called from KernelBackground.
 *
 * @param app_id App whose fetch is cancelled; @c INSTALL_ID_INVALID cancels any fetch.
 */
void app_fetch_cancel_from_system_task(AppInstallId app_id);

/**
 * @brief Check whether a fetch is in progress.
 *
 * @return true if a fetch is in progress.
 */
bool app_fetch_in_progress(void);

/**
 * @brief Handle a put_bytes event.
 *
 * Tracks the progress and completion of the fetched parts. Called from KernelMain; ignored if no
 * fetch is in progress.
 *
 * @param pb_event Put bytes event, copied before processing.
 */
void app_fetch_put_bytes_event_handler(PebblePutBytesEvent *pb_event);

/**
 * @brief Get the result of the last fetch.
 *
 * @return Result and app of the last fetch.
 */
AppFetchError app_fetch_get_previous_error(void);

/** @} */
