#pragma once

#include <pbl/services/imu/units.h>

#define BT_VENDOR_ID   0x0EEA
#define BT_VENDOR_NAME "Core Devices LLC"

#define BOARD_LSE_MODE RCC_LSE_Bypass

#define BOARD_RTC_INST NRF_RTC1
#define BOARD_RTC_IRQN RTC1_IRQn

extern const struct pbl_uart *const DBG_UART;

extern MicDevice *const MIC;
extern AudioDevice *const AUDIO;

extern const struct pbl_i2c_dev *const I2C_DA7212;
