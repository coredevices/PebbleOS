/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/normal/timeline/timeline_resources.h"

bool timeline_resources_get_id_system(TimelineResourceId timeline_id, TimelineResourceSize size,
                                      ResAppNum res_app_num, AppResourceInfo *res_info_out) {
  return false;
}

void timeline_resources_get_id(const TimelineResourceInfo *timeline_res, TimelineResourceSize size,
                               AppResourceInfo *res_info) { }

bool timeline_resources_is_system(TimelineResourceId timeline_id) {
  return false;
}
