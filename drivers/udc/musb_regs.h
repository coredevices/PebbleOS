/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/* Common registers */

#define MUSB_FADDR    0x00
#define MUSB_POWER    0x01
#define MUSB_INTRTX   0x02
#define MUSB_INTRRX   0x04
#define MUSB_INTRTXE  0x06
#define MUSB_INTRRXE  0x08
#define MUSB_INTRUSB  0x0a
#define MUSB_INTRUSBE 0x0b
#define MUSB_INDEX    0x0e

#define MUSB_POWER_HSMODE   PBL_BIT(4)
#define MUSB_POWER_HSENAB   PBL_BIT(5)
#define MUSB_POWER_SOFTCONN PBL_BIT(6)

#define MUSB_INTRUSB_SUSPEND PBL_BIT(0)
#define MUSB_INTRUSB_RESUME  PBL_BIT(1)
#define MUSB_INTRUSB_RESET   PBL_BIT(2)

/* Registers of the endpoint INDEX selects */

#define MUSB_TXMAXP  0x10
#define MUSB_CSR0    0x12
#define MUSB_TXCSR   0x12
#define MUSB_RXMAXP  0x14
#define MUSB_RXCSR   0x16
#define MUSB_COUNT0  0x18
#define MUSB_RXCOUNT 0x18

#define MUSB_CSR0_RXPKTRDY         PBL_BIT(0)
#define MUSB_CSR0_TXPKTRDY         PBL_BIT(1)
#define MUSB_CSR0_SENTSTALL        PBL_BIT(2)
#define MUSB_CSR0_DATAEND          PBL_BIT(3)
#define MUSB_CSR0_SETUPEND         PBL_BIT(4)
#define MUSB_CSR0_SENDSTALL        PBL_BIT(5)
#define MUSB_CSR0_SERVICEDRXPKTRDY PBL_BIT(6)
#define MUSB_CSR0_SERVICEDSETUPEND PBL_BIT(7)

#define MUSB_TXCSR_TXPKTRDY   PBL_BIT(0)
#define MUSB_TXCSR_FLUSHFIFO  PBL_BIT(3)
#define MUSB_TXCSR_SENDSTALL  PBL_BIT(4)
#define MUSB_TXCSR_SENTSTALL  PBL_BIT(5)
#define MUSB_TXCSR_CLRDATATOG PBL_BIT(6)
#define MUSB_TXCSR_MODE       PBL_BIT(13)
#define MUSB_TXCSR_ISO        PBL_BIT(14)

#define MUSB_RXCSR_RXPKTRDY   PBL_BIT(0)
#define MUSB_RXCSR_FLUSHFIFO  PBL_BIT(4)
#define MUSB_RXCSR_SENDSTALL  PBL_BIT(5)
#define MUSB_RXCSR_SENTSTALL  PBL_BIT(6)
#define MUSB_RXCSR_CLRDATATOG PBL_BIT(7)
#define MUSB_RXCSR_ISO        PBL_BIT(14)

/* FIFOs and dynamic FIFO sizing */

#define MUSB_FIFO(n)   (0x20 + 4U * (n))
#define MUSB_TXFIFOSZ  0x62
#define MUSB_RXFIFOSZ  0x63
#define MUSB_TXFIFOADD 0x64
#define MUSB_RXFIFOADD 0x66

#define MUSB_FIFOSZ_DPB PBL_BIT(4)

#define MUSB_RXDPKTBUFDIS 0x340
#define MUSB_TXDPKTBUFDIS 0x342

/* SiFli: the controller clock, its 60 MHz kernel clock or the 48 MHz crystal */
#define SIFLI_USBC_MODE_48M      0x3be
#define SIFLI_USBC_MODE_60M      0x2U
#define SIFLI_USBC_MODE_48M_XTAL 0x3U
#define SIFLI_USBC_60M_RATE      (60000000ULL * 1000ULL) /* in mHz */

/* SiFli: PHY configuration, with the session valid inputs */
#define SIFLI_USBC_USBCFG      0x370
#define SIFLI_USBCFG_AVALID_DR PBL_BIT(2)
#define SIFLI_USBCFG_AVALID    PBL_BIT(3)
