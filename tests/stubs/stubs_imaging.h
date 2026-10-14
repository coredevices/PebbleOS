/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/imaging.h>

extern ImagingReceivedHandler s_imaging_received_handlers[ImagingImageTypeCount];
extern ImagingWillReceiveHandler s_imaging_will_receive_handlers[ImagingImageTypeCount];
extern ImagingTransferFailedHandler s_imaging_transfer_failed_handlers[ImagingImageTypeCount];
