/*
 * Copyright 2025 Google LLC
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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "system/logging.h"

#define console_printf(_fmt, ...) PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_INFO, _fmt, ##__VA_ARGS__)

#endif /* __CONSOLE_H__ */
