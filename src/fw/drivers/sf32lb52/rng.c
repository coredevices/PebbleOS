/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/rng.h"
#include "system/logging.h"
#include "system/passert.h"

#include "bf0_hal_rcc.h"
#include "bf0_hal_rng.h"

static bool s_inited;
static RNG_HandleTypeDef s_rng_hdl = {
    .Instance = hwp_trng,
};

bool rng_rand(uint32_t *rand_out) {
  HAL_StatusTypeDef status;

  HAL_RCC_EnableModule(RCC_MOD_TRNG);

  if (!s_inited) {
    status = HAL_RNG_Init(&s_rng_hdl);
    PBL_ASSERTN(status == HAL_OK);

    status = HAL_RNG_GenerateRandomSeed(&s_rng_hdl, rand_out);
    PBL_ASSERTN(status == HAL_OK);

    s_inited = true;
  }

  status = HAL_RNG_GenerateRandomNumber(&s_rng_hdl, rand_out);
  if (status != HAL_OK) {
    PBL_LOG(LOG_LEVEL_ERROR, "HAL_RNG_GenerateRandomNumber failed: %d", status);
  }

  HAL_RCC_DisableModule(RCC_MOD_TRNG);

  return status == HAL_OK;
}
