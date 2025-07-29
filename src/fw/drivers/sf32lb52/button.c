/*
 * Copyright 2025 SiFli Technologies(Nanjing) Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "drivers/button.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/periph_config.h"
#include "drivers/gpio.h"
#include "kernel/events.h"
#include "system/passert.h"

bool button_is_pressed(ButtonId id) {
  const InputConfig config = {
    .gpio = BOARD_CONFIG_BUTTON.buttons[id].port,
    .gpio_pin = BOARD_CONFIG_BUTTON.buttons[id].pin,
  };
  uint32_t bit = gpio_input_read(&config);
  return (BOARD_CONFIG_BUTTON.buttons[id].active_high) ? bit : !bit;
}

uint8_t button_get_state_bits(void) {
  uint8_t button_state = 0x00;
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    button_state |= (button_is_pressed(i) ? 0x01 : 0x00) << i;
  }
  return button_state;
}

void button_init(void) {
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    const InputConfig config = {
      .gpio = BOARD_CONFIG_BUTTON.buttons[i].port,
      .gpio_pin = BOARD_CONFIG_BUTTON.buttons[i].pin,
    };
    gpio_input_init_pull_up_down(&config, BOARD_CONFIG_BUTTON.buttons[i].pull);
  }
}

bool button_selftest(void) {
  return button_get_state_bits() == 0;
}

void command_button_read(const char* button_id_str) {
  int button = atoi(button_id_str);

  if (button < 0 || button >= NUM_BUTTONS) {
    prompt_send_response("Invalid button");
    return;
  }

  if (button_is_pressed(button)) {
    prompt_send_response("down");
  } else {
    prompt_send_response("up");
  }
}
