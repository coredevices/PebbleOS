/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/pwr.h"

#include "drivers/periph_config.h"

#define STM32F4_COMPATIBLE
#include <mcu.h>

void pwr_enable_wakeup(bool enable) {
#if PLATFORM_SILK
  PWR_WakeUpPinCmd(PWR_WakeUp_Pin1, enable ? ENABLE : DISABLE);
#else
  PWR_WakeUpPinCmd(enable ? ENABLE : DISABLE);
#endif
}

void pwr_flash_power_down_stop_mode(bool power_down) {
  PWR_FlashPowerDownCmd(power_down ? ENABLE : DISABLE);
}

void pwr_access_backup_domain(bool enable_access) {
  periph_config_enable(PWR, RCC_APB1Periph_PWR);
  PWR_BackupAccessCmd(enable_access ? ENABLE : DISABLE);
  periph_config_disable(PWR, RCC_APB1Periph_PWR);
}
