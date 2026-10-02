/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "attribute.h"
#include "item.h"

#include "pbl/services/comm_session/session.h"
#include "pbl/util/uuid.h"

/**
 * @defgroup services_timeline_actions_endpoint Timeline action endpoint
 * @ingroup services_timeline
 * @brief Pebble Protocol endpoint 0x2cb0 for actions executed by the phone.
 *
 * The watch sends invoke action requests; the phone answers with a response that is posted as a
 * PebbleSysNotificationActionResult.
 * @{
 */

/**
 * @brief Ask the phone to invoke an action.
 *
 * @param id Id of the pin or notification.
 * @param type Type of the action; ANCS response and generic actions use the ANCS variant of the
 *             command.
 * @param action_id Id of the action.
 * @param attributes Extra attributes to send (e.g. the reply text), may be NULL.
 * @param do_async true to send from KernelBG, false to send from the current task.
 */
void timeline_action_endpoint_invoke_action(const Uuid *id, TimelineItemActionType type,
                                            uint8_t action_id, const AttributeList *attributes,
                                            bool do_async);

/**
 * @brief Handle a message from the phone on the timeline action endpoint.
 *
 * @param session Session the message arrived on.
 * @param data Message.
 * @param length Length of @p data in bytes.
 */
void timeline_action_endpoint_protocol_msg_callback(CommSession *session, const uint8_t *data,
                                                    size_t length);

/** @} */
