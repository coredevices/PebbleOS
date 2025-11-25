/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "launcher_app_glance.h"

#include "applib/ui/kino/kino_reel_custom.h"
#include "system/passert.h"
#include "util/struct.h"

GSize launcher_app_glance_get_size_for_reel(KinoReel *reel) {
  PBL_ASSERTN(reel->impl && (reel->impl->reel_type == KinoReelTypeCustom));
  LauncherAppGlance *glance = kino_reel_custom_get_data(reel);
  return NULL_SAFE_FIELD_ACCESS(glance, size, GSizeZero);
}
