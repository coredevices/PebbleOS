/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/*
 * The SF32LB57 MPI is the SF32LB52 one (sf32lb52_mpi.h) with calibration registers after MISCR,
 * which move the registers that follow, and a write timeout in the upper half of TIMR.
 */

/* Register offsets */

#define SF32LB57_MPI_CIR_OFFSET  0x0080
#define SF32LB57_MPI_TIMR_OFFSET 0x008c

/* Register bit definitions */

#define SF32LB57_MPI_MISCR_SCKINV PBL_BIT(16)
