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
typedef struct ImagingAlbumArt ImagingAlbumArt;
typedef struct CommSession CommSession;

//! Generic image-fetch service. Consumers ask the phone for an image and receive a bitmap or
//! segmented album art through a registered handler. Watch-pull only, capability-gated.

//! Called on KernelMain when a requested image finishes transferring. `bitmap` is NULL when the
//! phone reported it has no image (ImagingResponseFlagNoImage). Ownership of a non-NULL `bitmap`
//! (and its pixel/palette buffers) passes to the handler. `token` echoes the request.
typedef void (*ImagingReceivedHandler)(uint8_t token, struct GBitmap *bitmap);
typedef void (*ImagingAlbumArtHandler)(uint8_t token, ImagingAlbumArt *art);

//! The album-art receiver retains packed pixels in small kernel allocations. The caller owns art.
void imaging_register_album_art_handler(ImagingAlbumArtHandler handler);
void imaging_album_art_free(ImagingAlbumArt *art);
uint16_t imaging_album_art_width(const ImagingAlbumArt *art);
uint16_t imaging_album_art_height(const ImagingAlbumArt *art);
uint16_t imaging_album_art_row_size(const ImagingAlbumArt *art);
const uint8_t *imaging_album_art_palette(const ImagingAlbumArt *art);
//! Decode one tile (at most ten 4-bpp rows) into output. output_size must cover the tile.
bool imaging_album_art_decode_tile(const ImagingAlbumArt *art, uint16_t tile, uint8_t *output,
                                   size_t output_size);

//! Called before the first chunk's image buffers are allocated.
typedef void (*ImagingWillReceiveHandler)(uint8_t token);

//! Called when an image transfer is dropped before delivery.
typedef void (*ImagingTransferFailedHandler)(uint8_t token);

//! Register the handler for an image type. One handler per type; overwrites any previous.
void imaging_register_handler(ImagingImageType image_type, ImagingReceivedHandler handler);

//! Register transfer lifecycle handlers for an image type.
void imaging_register_transfer_handlers(ImagingImageType image_type,
                                        ImagingWillReceiveHandler will_receive,
                                        ImagingTransferFailedHandler transfer_failed);

//! True if the connected phone advertises image-fetch support and hasn't told us it can't serve
//! this image type (see ImagingResponseFlagUnsupported). Latched state resets on reconnect.
bool imaging_is_type_supported(ImagingImageType image_type);

//! Ask the phone for an album-art image for the named track, at the given size/format. No-op (and
//! returns false) if unsupported. The matching handler is invoked when the transfer completes.
bool imaging_request_album_art(uint8_t token, ImagingFormat format, uint16_t width, uint16_t height,
                               const char *title, const char *artist);

//! Ask the phone for the image it holds for timeline item `item_id`, at the given size/format.
//! No-op (and returns false) if unsupported. The matching handler is invoked when the transfer
//! completes.
bool imaging_request_notification_image(uint8_t token, ImagingFormat format, uint16_t width,
                                        uint16_t height, const Uuid *item_id);

//! Endpoint receive callback (registered in protocol_endpoints_table.json).
void imaging_protocol_msg_callback(CommSession *session, const uint8_t *msg, size_t length);

//! Comm-session event hook (called from the shell event loop): frees a partially received image
//! and clears the unsupported-type latch when the system session closes.
void imaging_handle_comm_session_event(const PebbleCommSessionEvent *event);
