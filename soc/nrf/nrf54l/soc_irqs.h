/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! nRF54L IRQ lines, the same on every core.

#pragma once

#define PBL_SOC_IRQN_SWI00       28
#define PBL_SOC_IRQN_SWI01       29
#define PBL_SOC_IRQN_SWI02       30
#define PBL_SOC_IRQN_SWI03       31
#define PBL_SOC_IRQN_SPU00       64
#define PBL_SOC_IRQN_MPC00       65
#define PBL_SOC_IRQN_VPR00       76
#define PBL_SOC_IRQN_CTRLAP      82
#define PBL_SOC_IRQN_CM33SS      84
#define PBL_SOC_IRQN_TIMER00     85
#define PBL_SOC_IRQN_SPU10       128
#define PBL_SOC_IRQN_TIMER10     133
#define PBL_SOC_IRQN_EGU10       135
#define PBL_SOC_IRQN_RADIO_0     138
#define PBL_SOC_IRQN_RADIO_1     139
#define PBL_SOC_IRQN_SPU20       192
#define PBL_SOC_IRQN_SERIAL20    198
#define PBL_SOC_IRQN_SERIAL21    199
#define PBL_SOC_IRQN_SERIAL22    200
#define PBL_SOC_IRQN_EGU20       201
#define PBL_SOC_IRQN_TIMER20     202
#define PBL_SOC_IRQN_TIMER21     203
#define PBL_SOC_IRQN_TIMER22     204
#define PBL_SOC_IRQN_TIMER23     205
#define PBL_SOC_IRQN_TIMER24     206
#define PBL_SOC_IRQN_PDM20       208
#define PBL_SOC_IRQN_PDM21       209
#define PBL_SOC_IRQN_PWM20       210
#define PBL_SOC_IRQN_PWM21       211
#define PBL_SOC_IRQN_PWM22       212
#define PBL_SOC_IRQN_SAADC       213
#define PBL_SOC_IRQN_NFCT        214
#define PBL_SOC_IRQN_TEMP        215
#define PBL_SOC_IRQN_GPIOTE20_0  218
#define PBL_SOC_IRQN_GPIOTE20_1  219
#define PBL_SOC_IRQN_QDEC20      224
#define PBL_SOC_IRQN_QDEC21      225
#define PBL_SOC_IRQN_GRTC_0      226
#define PBL_SOC_IRQN_GRTC_1      227
#define PBL_SOC_IRQN_GRTC_2      228
#define PBL_SOC_IRQN_GRTC_3      229
#define PBL_SOC_IRQN_SPU30       256
#define PBL_SOC_IRQN_SERIAL30    260
#define PBL_SOC_IRQN_COMP_LPCOMP 262
#define PBL_SOC_IRQN_WDT30       264
#define PBL_SOC_IRQN_WDT31       265
#define PBL_SOC_IRQN_GPIOTE30_0  268
#define PBL_SOC_IRQN_GPIOTE30_1  269

#if defined(CONFIG_SOC_NRF54L15)
#define PBL_SOC_IRQN_AAR00_CCM00 70
#define PBL_SOC_IRQN_ECB00       71
#define PBL_SOC_IRQN_CRACEN      72
#define PBL_SOC_IRQN_SERIAL00    74
#define PBL_SOC_IRQN_RRAMC       75
#define PBL_SOC_IRQN_TAMPC       220
#define PBL_SOC_IRQN_I2S20       221
#define PBL_SOC_IRQN_CLOCK_POWER 261
#elif defined(CONFIG_SOC_NRF54LM20A)
#define PBL_SOC_IRQN_AAR00_CCM00 74
#define PBL_SOC_IRQN_ECB00       75
#define PBL_SOC_IRQN_SERIAL00    77
#define PBL_SOC_IRQN_RRAMC       78
#define PBL_SOC_IRQN_EGU00       88
#define PBL_SOC_IRQN_CRACEN      89
#define PBL_SOC_IRQN_USBHS       90
#define PBL_SOC_IRQN_TDM         232
#define PBL_SOC_IRQN_SERIAL23    237
#define PBL_SOC_IRQN_SERIAL24    238
#define PBL_SOC_IRQN_TAMPC       239
#define PBL_SOC_IRQN_CLOCK_POWER 270
#define PBL_SOC_IRQN_VREGUSB     289
#endif
