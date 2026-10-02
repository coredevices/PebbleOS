/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kernel/events.h"
#include "pbl/services/imaging_endpoint_types.h"
#include "pbl/util/uuid.h"

struct GBitmap;
typedef struct CommSession CommSession;

/**
 * @defgroup services_imaging Image fetch
 * @ingroup services
 * @brief Fetches images from the phone.
 *
 * Consumers such as album art ask the phone for an image of a given size and pixel format; the
 * phone streams it back in chunks over the image-fetch endpoint (0x0035) and the reassembled
 * bitmap is passed to the handler registered for the image type. Requests are only sent when the
 * phone advertises image-fetch support.
 *
 * @code{.c}
 * static void prv_received(uint8_t token, struct GBitmap *bitmap) {
 *   // bitmap is NULL if the phone has none; otherwise the handler owns it
 * }
 *
 * imaging_register_handler(ImagingImageTypeAlbumArt, prv_received);
 * imaging_request_album_art(token, ImagingFormat8BitColor, 144, 144, title, artist);
 * @endcode
 * @{
 */

/**
 * @brief Called on KernelMain when a requested image has been received.
 *
 * @param token Token of the request.
 * @param bitmap Received image, or NULL if the phone has none (@ref ImagingResponseFlagNoImage).
 *               Ownership of a non-NULL bitmap and its pixel and palette buffers passes to the
 *               handler.
 */
typedef void (*ImagingReceivedHandler)(uint8_t token, struct GBitmap *bitmap);

/**
 * @brief Called before the buffers for an incoming image are allocated.
 *
 * @param token Token of the request.
 */
typedef void (*ImagingWillReceiveHandler)(uint8_t token);

/**
 * @brief Called when an image transfer is dropped before delivery.
 *
 * @param token Token of the request.
 */
typedef void (*ImagingTransferFailedHandler)(uint8_t token);

/**
 * @brief Register the handler for an image type.
 *
 * One handler per type; replaces any previous one. Images without a handler are freed.
 *
 * @param image_type Image type.
 * @param handler Handler, or NULL.
 */
void imaging_register_handler(ImagingImageType image_type, ImagingReceivedHandler handler);

/**
 * @brief Register the transfer lifecycle handlers for an image type.
 *
 * @param image_type Image type.
 * @param will_receive Called before buffers are allocated, or NULL.
 * @param transfer_failed Called when a transfer is dropped, or NULL.
 */
void imaging_register_transfer_handlers(ImagingImageType image_type,
                                        ImagingWillReceiveHandler will_receive,
                                        ImagingTransferFailedHandler transfer_failed);

/**
 * @brief Check whether the connected phone can serve an image type.
 *
 * @param image_type Image type.
 * @return true if the phone advertises image-fetch support and has not answered with
 *         @ref ImagingResponseFlagUnsupported for this type since it connected.
 */
bool imaging_is_type_supported(ImagingImageType image_type);

/**
 * @brief Request the album art of a track.
 *
 * The handler of @ref ImagingImageTypeAlbumArt is called when the transfer completes.
 *
 * @param token Opaque value passed back to the handlers.
 * @param format Pixel format.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param title Track title, truncated to 255 bytes, or NULL.
 * @param artist Track artist, truncated to 255 bytes, or NULL.
 * @return true if the request was sent, false if the type is unsupported.
 */
bool imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width, uint16_t height,
                               const char *title, const char *artist);

/**
 * @brief Request the image the phone holds for a timeline item.
 *
 * The handler of @ref ImagingImageTypeNotification is called when the transfer completes.
 *
 * @param token Opaque value passed back to the handlers.
 * @param format Pixel format.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param item_id UUID of the timeline item.
 * @return true if the request was sent, false if @p item_id is NULL or the type is unsupported.
 */
bool imaging_request_notification_image(uint8_t token, ImagingFormat format, uint16_t width,
                                        uint16_t height, const Uuid *item_id);

/**
 * @brief Handle a message received on the image-fetch endpoint.
 *
 * Registered in the protocol endpoints table.
 *
 * @param session Session the message was received on.
 * @param msg Message.
 * @param length Length of @p msg in bytes.
 */
void imaging_protocol_msg_callback(CommSession *session, const uint8_t *msg, size_t length);

/**
 * @brief Handle a comm session event.
 *
 * Called from the shell event loop. When the system session closes, frees a partially received
 * image and forgets the image types the phone reported as unsupported.
 *
 * @param event Comm session event.
 */
void imaging_handle_comm_session_event(const PebbleCommSessionEvent *event);

/** @} */
