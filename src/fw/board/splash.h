/*
 * Copyright 2025 Core Devices LLC
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

#pragma once

#if BOARD_OBELIX_EVT || BOARD_OBELIX_DVT || BOARD_OBELIX_PVT || BOARD_OBELIX_BB || BOARD_OBELIX_BB2
#include "splash/splash_obelix.xbm"
#else
#error "Unknown splash definition for board"
#endif // BOARD_*
