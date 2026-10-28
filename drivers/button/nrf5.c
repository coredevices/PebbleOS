#include <pbl/drivers/button.h>
#include <pbl/drivers/gpio.h>

#include <board/board.h>
#include <kernel/events.h>

bool button_is_pressed(ButtonId id) {
  return pbl_gpio_get(&BOARD_CONFIG_BUTTON.buttons[id].gpio) > 0;
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
    pbl_gpio_configure(&BOARD_CONFIG_BUTTON.buttons[i].gpio, PBL_GPIO_INPUT);
  }
}
