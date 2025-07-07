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

#include "drivers/rng.h"

#include "board/board.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "system/passert.h"

static bool s_init = false;
static RNG_HandleTypeDef s_rng_hdl = {
    .Instance = hwp_trng,
};
bool rng_rand(uint32_t *rand_out) {
  PBL_ASSERTN(rand_out);
  HAL_StatusTypeDef status;

  if (!s_init) {
    status = HAL_RNG_Init(&s_rng_hdl);
    if (status != HAL_OK) {
      return false;
    }
    s_init = true;
  }
  status = HAL_RNG_Generate(&s_rng_hdl, rand_out, 0);
  return true;
}
