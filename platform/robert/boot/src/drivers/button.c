/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/button.h"

#include "board/board.h"
#include "drivers/periph_config.h"
#include "drivers/gpio.h"

static void prv_initialize_button(const ButtonConfig* config) {
  // Configure the pin itself
  gpio_input_init_pull_up_down(&config->input, config->pupd);
}

bool button_is_pressed(ButtonId id) {
  const ButtonConfig* button_config = &BOARD_CONFIG_BUTTON.buttons[id];
  return !gpio_input_read(&button_config->input);
}

uint8_t button_get_state_bits(void) {
  uint8_t button_state = 0x00;
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    button_state |= (button_is_pressed(i) ? 0x01 : 0x00) << i;
  }
  return button_state;
}

void button_init(void) {
  periph_config_acquire_lock();

  for (int i = 0; i < NUM_BUTTONS; ++i) {
    prv_initialize_button(&BOARD_CONFIG_BUTTON.buttons[i]);
  }

  periph_config_release_lock();
}
