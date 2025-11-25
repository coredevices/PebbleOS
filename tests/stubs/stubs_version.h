/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "system/version.h"
#include "git_version.auto.h"

const FirmwareMetadata TINTIN_METADATA = {
  .version_timestamp = GIT_TIMESTAMP,
  .version_tag = GIT_TAG,
  .version_short = GIT_REVISION,
  .is_recovery_firmware = false,
  .hw_platform = FirmwareMetadataPlatformPebbleTwoPointZero,
  .metadata_version = FW_METADATA_CURRENT_STRUCT_VERSION,
};
