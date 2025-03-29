#pragma once

#include <stdbool.h>

void watchdog_init(void);
void watchdog_start(void);

bool watchdog_check_clear_reset_flag(void);
