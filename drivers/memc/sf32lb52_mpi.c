/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/memc/sf32lb52_mpi.h>

int pbl_sf32lb52_mpi_prepare(const struct pbl_sf32lb52_mpi *mpi) {
  int ret;

  if (mpi->state->prepared) {
    return 0;
  }

  if (mpi->pinctrl.states[PBL_PINCTRL_STATE_DEFAULT] != nullptr) {
    ret = pbl_pinctrl_select_state(&mpi->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_clk_assign(mpi->assigned);
  if (ret != 0) {
    return ret;
  }
  mpi->state->prepared = true;

  return 0;
}

#include PBL_DT_INSTANCES(sifli_sf32lb52_mpi_sf32lb52_mpi)
