/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* CSRs of the Nordic VPR */

// Core control, which writes need the key in
#define NRF_VPR_CSR_CTRL            0x7c0
#define NRF_VPR_CTRL_ENABLERTPERIPH (1U << 0)
#define NRF_VPR_CTRL_KEY            (0x507dU << 16)
#define NRF_VPR_CTRL_KEY_MSK        (0xffffU << 16)

// VTIM, two 16-bit down-counters at the core clock, which combine into one of 32 bits
#define NRF_VPR_CSR_CNTMODE0    0x7d0
#define NRF_VPR_CSR_CNTMODE1    0x7d1
#define NRF_VPR_CSR_CNT         0x7d2
#define NRF_VPR_CNTMODE_WRAP    1U
#define NRF_VPR_CNTMODE_COMBINE 3U

// VEVIF tasks, each driving the CLIC line of its number
#define NRF_VPR_CSR_TASKS 0x7e0
