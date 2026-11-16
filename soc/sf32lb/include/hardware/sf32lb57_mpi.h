/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/*
 * The SF32LB57 MPI is the SF32LB52 one (sf32lb52_mpi.h) with calibration registers after MISCR,
 * which move the registers that follow, and a write timeout in the upper half of TIMR. DCR selects
 * the memory protocol in one field, and MISCR has no receive clock delay.
 */

/* Register offsets */

#define SF32LB57_MPI_CALCR_OFFSET 0x005c
#define SF32LB57_MPI_CALRR_OFFSET 0x0060
#define SF32LB57_MPI_CIR_OFFSET   0x0080
#define SF32LB57_MPI_TIMR_OFFSET  0x008c
#define SF32LB57_MPI_WDTR_OFFSET  0x0090

/* Register bit definitions */

#define SF32LB57_MPI_DCR_PROT_MASK     PBL_GENMASK(5, 4)
#define SF32LB57_MPI_DCR_PROT_HYPERBUS 2U

#define SF32LB57_MPI_MISCR_DQSDLY_MASK PBL_GENMASK(7, 0)
#define SF32LB57_MPI_MISCR_SCKDLY_MASK PBL_GENMASK(15, 8)
#define SF32LB57_MPI_MISCR_SCKINV      PBL_BIT(16)

#define SF32LB57_MPI_CALCR_STOP  PBL_BIT(30)
#define SF32LB57_MPI_CALCR_START PBL_BIT(31)

#define SF32LB57_MPI_CALRR_CSMP PBL_BIT(16)
