#pragma once

#include <stdbool.h>

void watchdog_init(void);
void watchdog_kick(void);

bool watchdog_check_clear_reset_flag(void);
