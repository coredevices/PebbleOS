/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/comm_session/session.h>

/**
 * @defgroup services_compositor_screenshot_pp Screenshot protocol
 * @ingroup services_compositor
 * @brief Pebble Protocol endpoint that sends the system framebuffer to the phone.
 * @{
 */

/**
 * @brief Handle a screenshot request from the client.
 *
 * Freezes the compositor and streams the framebuffer back in chunks. Requests received while a
 * screenshot is in progress, or with an unknown command, are answered with an error.
 *
 * @param session Session the request came from.
 * @param data Message payload.
 * @param length Length of @p data in bytes.
 */
void screenshot_protocol_msg_callback(CommSession *session, const uint8_t *data,
                                      unsigned int length);

/** @} */
