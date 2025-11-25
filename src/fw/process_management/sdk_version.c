/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sdk_version.h"

bool sdk_version_is_app_messaging_supported(const Version * const sdk_version) {
  return ((sdk_version->major == 3 && sdk_version->minor >= 1) ||
          sdk_version->major > 3);
}
