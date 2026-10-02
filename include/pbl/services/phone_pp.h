/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup services_phone_pp Phone endpoint
 * @ingroup services
 * @brief Pebble Protocol phone control endpoint (0x21).
 *
 * Sends call control commands to the phone over the system session. If no session exists, a
 * call disconnect event is emitted instead.
 * @{
 */

/**
 * @brief Ask the phone to answer a call.
 *
 * @param cookie Call identifier from the phone event.
 */
void pp_answer_call(uint32_t cookie);

/**
 * @brief Ask the phone to decline or hang up a call.
 *
 * @param cookie Call identifier from the phone event.
 */
void pp_decline_call(uint32_t cookie);

/** @brief Request the state of the phone's calls. */
void pp_get_phone_state(void);

/**
 * @brief Enable or disable handling of Get Phone State responses.
 *
 * Used to ignore stray responses still in flight after the user declined the call from the
 * watch.
 *
 * @param enabled True to handle responses, false to drop them.
 */
void pp_get_phone_state_set_enabled(bool enabled);

/** @} */
