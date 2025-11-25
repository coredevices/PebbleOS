/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define RTC_BKP_BOOTBIT_DR                      RTC_BKP_DR0
#define STUCK_BUTTON_REGISTER                   RTC_BKP_DR1
#define BOOTLOADER_VERSION_REGISTER             RTC_BKP_DR2
#define CURRENT_TIME_REGISTER                   RTC_BKP_DR3
#define CURRENT_INTERVAL_TICKS_REGISTER         RTC_BKP_DR4
#define REBOOT_REASON_REGISTER_1                RTC_BKP_DR5
#define REBOOT_REASON_REGISTER_2                RTC_BKP_DR6
#define REBOOT_REASON_STUCK_TASK_PC             RTC_BKP_DR7
#define REBOOT_REASON_STUCK_TASK_LR             RTC_BKP_DR8
#define REBOOT_REASON_STUCK_TASK_CALLBACK       RTC_BKP_DR9
#define REBOOT_REASON_MUTEX_LR                  RTC_BKP_DR10
#define REBOOT_REASON_MUTEX_PC                  RTC_BKP_DR11 // Deprecated
#define SLOT_OF_LAST_LAUNCHED_APP               RTC_BKP_DR19
