/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "rocky_api.h"
#include "jerry-api.h"

#include "applib/graphics/gtypes.h"

extern const RockyGlobalAPI GRAPHIC_APIS;

GContext *rocky_api_graphics_get_gcontext(void);
