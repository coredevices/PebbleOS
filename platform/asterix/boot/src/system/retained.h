#pragma once

#include <stdint.h>

#define RTC_BKP_BOOTBIT_DR                      0
#define STUCK_BUTTON_REGISTER                   1
#define BOOTLOADER_VERSION_REGISTER             2
#define CURRENT_TIME_REGISTER                   3
#define CURRENT_INTERVAL_TICKS_REGISTER         4
#define REBOOT_REASON_REGISTER_1                5
#define REBOOT_REASON_REGISTER_2                6
#define REBOOT_REASON_STUCK_TASK_PC             7
#define REBOOT_REASON_STUCK_TASK_LR             8
#define REBOOT_REASON_STUCK_TASK_CALLBACK       9
#define REBOOT_REASON_MUTEX_LR                  10
#define REBOOT_REASON_MUTEX_PC                  11 // Deprecated
#define SLOT_OF_LAST_LAUNCHED_APP               19
#define NRF_RETAINED_REGISTER_CRC               31

void retained_init();
void retained_write(uint8_t id, uint32_t value);
uint32_t retained_read(uint8_t id);
