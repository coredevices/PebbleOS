/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/button.h>
#include <pbl/drivers/debounced_button.h>
#include <pbl/input/input.h>
#include <pbl/kernel/irq.h>

#include <board/board.h>
#include <cmsis_core.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))

// QEMU GPIO register offsets (must match pebble-gpio device)
#define GPIO_BTN_STATE 0x00
#define GPIO_BTN_EDGE  0x04
#define GPIO_INTCTRL   0x08
#define GPIO_INTSTAT   0x0C

// Button bit positions (must match QEMU pebble-gpio device)
#define BTN_BIT_BACK   (1 << 0)
#define BTN_BIT_UP     (1 << 1)
#define BTN_BIT_SELECT (1 << 2)
#define BTN_BIT_DOWN   (1 << 3)

static const uint32_t s_button_bits[NUM_BUTTONS] = {
  [BUTTON_ID_BACK] = BTN_BIT_BACK,
  [BUTTON_ID_UP] = BTN_BIT_UP,
  [BUTTON_ID_SELECT] = BTN_BIT_SELECT,
  [BUTTON_ID_DOWN] = BTN_BIT_DOWN,
};

static uint32_t s_last_state;

static void prv_gpio_irq_handler(void) {
  uint32_t base = QEMU_GPIO_BASE;

  // Read which buttons changed
  uint32_t edge = REG32(base + GPIO_BTN_EDGE);
  // Clear edge flags
  REG32(base + GPIO_INTSTAT) = edge;

  // Read current state
  uint32_t state = REG32(base + GPIO_BTN_STATE);
  uint32_t changed = s_last_state ^ state;
  s_last_state = state;

  // Generate button events for each changed button
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (changed & s_button_bits[i]) {
      bool is_pressed = (state & s_button_bits[i]) != 0;
      pbl_input_report_key(BOARD_CONFIG_BUTTON.buttons[i].code, is_pressed, true);
    }
  }
}

PBL_IRQ_CONNECT(GPIO, 6, prv_gpio_irq_handler, , 0);

void button_init(void) {
  uint32_t base = QEMU_GPIO_BASE;

  // Clear any pending edge flags
  REG32(base + GPIO_INTSTAT) = 0xF;

  // Enable edge interrupt
  REG32(base + GPIO_INTCTRL) = 1;

  pbl_irq_enable(PBL_IRQN(GPIO));

  s_last_state = REG32(base + GPIO_BTN_STATE);
}

bool button_is_pressed(ButtonId id) {
  if (id >= NUM_BUTTONS) {
    return false;
  }
  uint32_t state = REG32(QEMU_GPIO_BASE + GPIO_BTN_STATE);
  return (state & s_button_bits[id]) != 0;
}

uint8_t button_get_state_bits(void) {
  return (uint8_t)(REG32(QEMU_GPIO_BASE + GPIO_BTN_STATE) & 0xF);
}

void button_set_rotated(bool rotated) {
  (void)rotated;
}

void debounced_button_init(void) {
  // QEMU handles debounce in the GPIO device itself
  button_init();
}

#ifdef CONFIG_SHELL
#include <errno.h>

#include <pbl/shell/shell.h>

#ifdef CONFIG_RECOVERY_FW
static int prv_cmd_button_read(const struct pbl_shell *sh, size_t argc, char **argv) {
  long button;

  if (pbl_shell_strtol(argv[1], &button) != 0 || button < 0 || button >= NUM_BUTTONS) {
    pbl_shell_error(sh, "invalid button '%s'", argv[1]);
    return -EINVAL;
  }

  pbl_shell_print(sh, "%s", button_is_pressed((ButtonId)button) ? "down" : "up");
  return 0;
}

PBL_SHELL_SUBCMD_ADD(sub_button, read, NULL, "Read the state of button <id>", prv_cmd_button_read,
                     2, 0);
#endif
#endif
