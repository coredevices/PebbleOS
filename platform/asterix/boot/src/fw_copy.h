#pragma once

#include <stdbool.h>

void check_update_fw(void);

//! @return false if we've failed to load recovery mode too many times and we should just sadwatch
bool switch_to_recovery_fw(void);
