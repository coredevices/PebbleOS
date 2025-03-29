#pragma once

#include "button_id.h"

#include <stdbool.h>
#include <stdint.h>

void button_init(void);

bool button_is_pressed(ButtonId id);
uint8_t button_get_state_bits(void);
