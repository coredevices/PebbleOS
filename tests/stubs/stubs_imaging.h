/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/imaging.h"
#include "pbl/kernel/compiler.h"

static ImagingReceivedHandler s_imaging_received_handlers[ImagingImageTypeCount];
static ImagingWillReceiveHandler s_imaging_will_receive_handlers[ImagingImageTypeCount];
static ImagingTransferFailedHandler s_imaging_transfer_failed_handlers[ImagingImageTypeCount];

void PBL_WEAK imaging_register_handler(ImagingImageType image_type,
                                       ImagingReceivedHandler handler) {
  s_imaging_received_handlers[image_type] = handler;
}

void PBL_WEAK imaging_register_transfer_handlers(ImagingImageType image_type,
                                                 ImagingWillReceiveHandler will_receive,
                                                 ImagingTransferFailedHandler transfer_failed) {
  s_imaging_will_receive_handlers[image_type] = will_receive;
  s_imaging_transfer_failed_handlers[image_type] = transfer_failed;
}

bool PBL_WEAK imaging_is_type_supported(ImagingImageType image_type) {
  return false;
}

bool PBL_WEAK imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width,
                                        uint16_t height, const char *title, const char *artist) {
  return false;
}

bool PBL_WEAK imaging_request_notification_image(uint8_t token, ImagingFormat format,
                                                 uint16_t width, uint16_t height,
                                                 const Uuid *item_id) {
  return false;
}
