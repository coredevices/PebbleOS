/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// 0x00 - 0x06: reserved
static const uint8_t LIS3DH_STATUS_REG_AUX = 0x07;
static const uint8_t LIS3DH_OUT_ADC1_L = 0x08;
static const uint8_t LIS3DH_OUT_ADC1_H = 0x09;
static const uint8_t LIS3DH_OUT_ADC2_L = 0x0a;
static const uint8_t LIS3DH_OUT_ADC2_H = 0x0b;
static const uint8_t LIS3DH_OUT_ADC3_L = 0x0c;
static const uint8_t LIS3DH_OUT_ADC3_H = 0x0d;
static const uint8_t LIS3DH_INT_COUNTER_REG = 0x0e;
static const uint8_t LIS3DH_WHO_AM_I = 0x0f;
// 0x10 - 0x1E: reserved
static const uint8_t LIS3DH_TEMP_CFG_REG = 0x1f;
static const uint8_t LIS3DH_CTRL_REG1 = 0x20;
static const uint8_t LIS3DH_CTRL_REG2 = 0x21;
static const uint8_t LIS3DH_CTRL_REG3 = 0x22;
static const uint8_t LIS3DH_CTRL_REG4 = 0x23;
static const uint8_t LIS3DH_CTRL_REG5 = 0x24;
static const uint8_t LIS3DH_CTRL_REG6 = 0x25;
static const uint8_t LIS3DH_REFERENCE = 0x26;
static const uint8_t LIS3DH_STATUS_REG2 = 0x27;
static const uint8_t LIS3DH_OUT_X_L = 0x28;
static const uint8_t LIS3DH_OUT_X_H = 0x29;
static const uint8_t LIS3DH_OUT_Y_L = 0x2a;
static const uint8_t LIS3DH_OUT_Y_H = 0x2b;
static const uint8_t LIS3DH_OUT_Z_L = 0x2c;
static const uint8_t LIS3DH_OUT_Z_H = 0x2d;
static const uint8_t LIS3DH_FIFO_CTRL_REG = 0x2e;
static const uint8_t LIS3DH_FIFO_SRC_REG = 0x2f;
static const uint8_t LIS3DH_INT1_CFG = 0x30;
static const uint8_t LIS3DH_INT1_SRC = 0x31;
static const uint8_t LIS3DH_INT1_THS = 0x32;
static const uint8_t LIS3DH_INT1_DURATION = 0x33;
// 0x34 - 0x37: reserved
static const uint8_t LIS3DH_CLICK_CFG = 0x38;
static const uint8_t LIS3DH_CLICK_SRC = 0x39;
static const uint8_t LIS3DH_CLICK_THS = 0x3a;
static const uint8_t LIS3DH_TIME_LIMIT = 0x3b;
static const uint8_t LIS3DH_TIME_LATENCY = 0x3c;
static const uint8_t LIS3DH_TIME_WINDOW = 0x3d;
static const uint8_t LIS3DH_ACT_THS = 0x3e;
static const uint8_t LIS3DH_INACT_DUR = 0x3f;

// CTRL_REG1
static const uint8_t ODR3 = (1 << 7);
static const uint8_t ODR2 = (1 << 6);
static const uint8_t ODR1 = (1 << 5);
static const uint8_t ODR0 = (1 << 4);
static const uint8_t ODR_MASK = (0xf0);
static const uint8_t LPen = (1 << 3);
static const uint8_t Zen = (1 << 2);
static const uint8_t Yen = (1 << 1);
static const uint8_t Xen = (1 << 0);

// CTRL_REG3
static const uint8_t I1_CLICK = (1 << 7);
static const uint8_t I1_AOI1 = (1 << 6);
static const uint8_t I1_DTRDY = (1 << 4);
static const uint8_t I1_WTM = (1 << 2);
static const uint8_t I1_OVRN = (1 << 1);

//CTRL_REG4
static const uint8_t BDU = (1 << 7);
static const uint8_t BLE = (1 << 6);
static const uint8_t FS1 = (1 << 5);
static const uint8_t FS0 = (1 << 4);
static const uint8_t HR = (1 << 3);
static const uint8_t ST1 = (1 << 2);
static const uint8_t ST0 = (1 << 1);
static const uint8_t SIM = (1 << 0);
static const uint8_t FS_MASK = 0x30;

//CTRL_REG5
static const uint8_t FIFO_EN = (1 << 6);

//CTRL_REG6
static const uint8_t I2_CLICK = (1 << 7);

// CLICK_CFG
static const uint8_t ZD = (1 << 5);
static const uint8_t ZS = (1 << 4);
static const uint8_t YD = (1 << 3);
static const uint8_t YS = (1 << 2);
static const uint8_t XD = (1 << 1);
static const uint8_t XS = (1 << 0);

// CLICK_SRC
static const uint8_t IA = 1 << 6;
static const uint8_t DCLICK = 1 << 5;
static const uint8_t SCLICK = 1 << 4;
static const uint8_t Sign = 1 << 3;
static const uint8_t ZClick = 1 << 2;
static const uint8_t YClick = 1 << 1;
static const uint8_t XClick = 1 << 0;

// FIFO_CTRL_REG
static const uint8_t MODE_BYPASS = 0x0;
static const uint8_t MODE_FIFO = (0x1 << 6);
static const uint8_t MODE_STREAM = (0x1 << 7);
static const uint8_t MODE_MASK = 0xc0;
static const uint8_t THR_MASK = 0x1f;

// FIFO_SRC_REG
static const uint8_t FIFO_WTM = (0x1 << 7);
static const uint8_t FIFO_OVRN = (0x1 << 6);
static const uint8_t FIFO_EMPTY = (0x1 << 5);
static const uint8_t FSS_MASK = 0x1f;
