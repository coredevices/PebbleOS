/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kino_reel.h"

KinoReel *kino_reel_custom_create(const KinoReelImpl *custom_impl, void *data);

void *kino_reel_custom_get_data(KinoReel *reel);
