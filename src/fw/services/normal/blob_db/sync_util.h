/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/blob_db/api.h"
#include "services/normal/settings/settings_file.h"

// Caution: CommonTimelineItemHeader .flags & .status are stored inverted and not auto-restored
// by the underlying db API. If .flags or .status is used from a CommonTimelineItemHeader below,
// be very careful.

//! A settings file each callback which checks if the there are dirty records in the file
//! @param context The address of a bool which will get set
bool sync_util_is_dirty_cb(SettingsFile *file, SettingsRecordInfo *info, void *context);

//! A settings file each callback which builds a BlobDBDirtyItem list
//! @param context The address of an empty dirty list which will get built
bool sync_util_build_dirty_list_cb(SettingsFile *file, SettingsRecordInfo *info, void *context);
