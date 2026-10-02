/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "touch_event.h"

/**
 * @addtogroup services_touch
 * @{
 */

/**
 * @brief Touch event callback.
 *
 * @param event Touch event.
 * @param context Callback context.
 */
typedef void (*TouchEventHandler)(const TouchEvent *event, void *context);

/** @} */
