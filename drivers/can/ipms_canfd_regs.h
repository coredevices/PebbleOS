/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

// The SiFli integration packs the byte registers of the core in words, and its frame buffers have
// no time stamps

#define CANFD_RBUF    0x00
#define CANFD_TBUF    0x48
#define CANFD_CR      0x90
#define CANFD_IR      0x94
#define CANFD_BITTIME 0x98
#define CANFD_PRESC   0x9c
#define CANFD_CNT     0xa0
#define CANFD_ACFCTRL 0xa4
#define CANFD_ACF     0xa8
#define CANFD_CR2     0xb0

// Frame buffer words: identifier, control, then data
#define CANFD_BUF_CTRL_DLC PBL_GENMASK(3, 0)
#define CANFD_BUF_CTRL_RTR PBL_BIT(6)
#define CANFD_BUF_CTRL_IDE PBL_BIT(7)
#define CANFD_BUF_DATA     2U

// CFG_STAT, TCMD, TCTRL and RCTRL
#define CANFD_CR_BUSOFF PBL_BIT(0)
#define CANFD_CR_TPSS   PBL_BIT(4)
#define CANFD_CR_RESET  PBL_BIT(7)
#define CANFD_CR_TSA    PBL_BIT(8)
#define CANFD_CR_TSALL  PBL_BIT(9)
#define CANFD_CR_TSONE  PBL_BIT(10)
#define CANFD_CR_TPA    PBL_BIT(11)
#define CANFD_CR_TPE    PBL_BIT(12)
#define CANFD_CR_LOM    PBL_BIT(14)
#define CANFD_CR_TSNEXT PBL_BIT(22)
#define CANFD_CR_RSTAT  PBL_GENMASK(25, 24)
#define CANFD_CR_RREL   PBL_BIT(28)
// Bits that start an action when written as 1
#define CANFD_CR_CMDS                                                                \
  (CANFD_CR_BUSOFF | CANFD_CR_TSA | CANFD_CR_TSALL | CANFD_CR_TSONE | CANFD_CR_TPA | \
   CANFD_CR_TPE | CANFD_CR_TSNEXT | CANFD_CR_RREL)

// RTIE, RTIF, ERRINT and LIMIT
#define CANFD_IR_EIE   PBL_BIT(1)
#define CANFD_IR_TPIE  PBL_BIT(3)
#define CANFD_IR_RFIE  PBL_BIT(5)
#define CANFD_IR_ROIE  PBL_BIT(6)
#define CANFD_IR_RIE   PBL_BIT(7)
#define CANFD_IR_AIF   PBL_BIT(8)
#define CANFD_IR_EIF   PBL_BIT(9)
#define CANFD_IR_TPIF  PBL_BIT(11)
#define CANFD_IR_RFIF  PBL_BIT(13)
#define CANFD_IR_ROIF  PBL_BIT(14)
#define CANFD_IR_RIF   PBL_BIT(15)
#define CANFD_IR_BEIF  PBL_BIT(16)
#define CANFD_IR_BEIE  PBL_BIT(17)
#define CANFD_IR_ALIF  PBL_BIT(18)
#define CANFD_IR_ALIE  PBL_BIT(19)
#define CANFD_IR_EPIF  PBL_BIT(20)
#define CANFD_IR_EPIE  PBL_BIT(21)
#define CANFD_IR_EPASS PBL_BIT(22)
#define CANFD_IR_EWARN PBL_BIT(23)
#define CANFD_IR_EWL   PBL_GENMASK(27, 24)
// Flags, cleared by writing 1
#define CANFD_IR_FLAGS (PBL_GENMASK(15, 8) | CANFD_IR_BEIF | CANFD_IR_ALIF | CANFD_IR_EPIF)

#define CANFD_BITTIME_S_SEG_1 PBL_GENMASK(5, 0)
#define CANFD_BITTIME_S_SEG_2 PBL_GENMASK(12, 8)
#define CANFD_BITTIME_S_SJW   PBL_GENMASK(19, 16)

#define CANFD_PRESC_S_PRESC PBL_GENMASK(7, 0)

// EALCAP, then RECNT and TECNT
#define CANFD_CNT_KOER  PBL_GENMASK(7, 5)
#define CANFD_CNT_RECNT PBL_GENMASK(23, 16)
#define CANFD_CNT_TECNT PBL_GENMASK(31, 24)

#define CANFD_KOER_BIT   1U
#define CANFD_KOER_FORM  2U
#define CANFD_KOER_STUFF 3U
#define CANFD_KOER_ACK   4U
#define CANFD_KOER_CRC   5U

#define CANFD_ACFCTRL_ACFADR  PBL_GENMASK(3, 0)
#define CANFD_ACFCTRL_SELMASK PBL_BIT(5)
#define CANFD_ACFCTRL_AE(n)   PBL_BIT(16U + (n))

#define CANFD_ACF_ACODE PBL_GENMASK(28, 0)
#define CANFD_ACF_AIDE  PBL_BIT(29)
#define CANFD_ACF_AIDEE PBL_BIT(30)
