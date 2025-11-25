/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if CAPABILITY_HAS_BUILTIN_HRM

typedef struct BLEHRMSharingRequest BLEHRMSharingRequest;

//! @note Must be called from KernelMain
void ble_hrm_push_sharing_request_window(BLEHRMSharingRequest *sharing_request);

#endif
