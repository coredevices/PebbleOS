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

bool rng_rand(uint32_t *rand_out) {
  PBL_ASSERTN(rand_out);
  RNG_HandleTypeDef rng_hdl = {0};
  HAL_StatusTypeDef status;

  rng_hdl.Instance = hwp_trng;

  status = HAL_RNG_Init(&rng_hdl);
  if (status != HAL_OK) {
    PBL_LOG(LOG_LEVEL_ERROR, "rng_rand init fail!");
    return false;
  }

  status = HAL_RNG_Generate(&rng_hdl, rand_out, 1);
  if (status != HAL_OK) {
    HAL_RNG_DeInit(&rng_hdl);
    PBL_LOG(LOG_LEVEL_ERROR, "rnd_rand generate fail! %d", status);
    return false;
  }

  HAL_RNG_DeInit(&rng_hdl);

  return true;
}
