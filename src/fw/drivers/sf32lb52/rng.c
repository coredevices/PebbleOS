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

#include "system/passert.h"

#define SF32LB52_COMPATIBLE
#include "mcu.h"
static bool s_inited;

static RNG_HandleTypeDef s_rng_hdl = {
    .Instance = hwp_trng,
};

bool rng_rand(uint32_t *rand_out) {
  PBL_ASSERTN(rand_out);
  HAL_StatusTypeDef status;
  if (!s_inited) {
    status = HAL_RNG_Init(&s_rng_hdl);
    if (status != HAL_OK) {
      PBL_LOG(LOG_LEVEL_ERROR, "rng_rand init fail!");
      return false;
    }
    s_inited = true;
  }
  
  HAL_RCC_EnableModule(RCC_MOD_TRNG);
  status = HAL_RNG_Generate(&s_rng_hdl, rand_out, 0);
  HAL_RCC_DisableModule(RCC_MOD_TRNG);
  
  if (status != HAL_OK) {
    HAL_RNG_DeInit(&s_rng_hdl);
    s_inited = false;
    PBL_LOG(LOG_LEVEL_ERROR, "rnd_rand generate fail! %d", status);
    return false;
  }
  return true;
}
