/* SPDX-FileCopyrightText: 2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/button.h>
#include <pbl/drivers/gpio.h>

#include <board/board.h>
#include <kernel/events.h>
#include <system/passert.h>

static bool s_rotated_180 = false;

void button_set_rotated(bool rotated_180) {
  s_rotated_180 = rotated_180;
}

bool button_is_pressed(ButtonId id) {
  if (s_rotated_180 && (id == BUTTON_ID_UP)) {
    id = BUTTON_ID_DOWN;
  } else if (s_rotated_180 && (id == BUTTON_ID_DOWN)) {
    id = BUTTON_ID_UP;
  }

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

#if defined(CONFIG_SHELL) && defined(CONFIG_RECOVERY_FW)
#include <errno.h>

#include <pbl/shell/shell.h>

static int prv_cmd_button_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  long button;

  if (pbl_shell_strtol(argv[1], &button) != 0 || button < 0 || button >= NUM_BUTTONS) {
    pbl_shell_error(sh, "invalid button '%s'", argv[1]);
    return -EINVAL;
  }

  pbl_shell_print(sh, "%s", button_is_pressed((ButtonId)button) ? "down" : "up");
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_button, read, nullptr, "Read the state of button <id>",
                     prv_cmd_button_read, 2, 0);
#endif
