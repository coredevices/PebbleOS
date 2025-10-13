/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tictoc.h"

#include "resource/resource_ids.auto.h"

const PebbleProcessMd* tictoc_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_md = {
    .common = {
      // UUID: 8f3c8686-31a1-4f5f-91f5-01600c9bdc59
      .uuid = { 0x8f, 0x3c, 0x86, 0x86, 0x31, 0xa1, 0x4f, 0x5f,
                0x91, 0xf5, 0x01, 0x60, 0x0c, 0x9b, 0xdc, 0x59 },
      .main_func = tictoc_main,
      .process_type = ProcessTypeWatchface,
#if CAPABILITY_HAS_ROCKY_JS && !defined(PLATFORM_SPALDING)
      .is_rocky_app = true,
#endif
    },
    .icon_resource_id = RESOURCE_ID_MENU_ICON_TICTOC_WATCH,
    .name = "TicToc",
  };
  return (const PebbleProcessMd*) &s_app_md;
}
