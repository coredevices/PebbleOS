#include "drivers/button.h"

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/periph_config.h"
#include "drivers/gpio.h"
#include "kernel/events.h"
#include "system/passert.h"

bool button_is_pressed(ButtonId id) {
  const ButtonConfig* button_config = &BOARD_CONFIG_BUTTON.buttons[id];
  
  uint32_t bit = gpio_input_read(&button_config->gpioi);
  return (BOARD_CONFIG_BUTTON.active_high) ? bit : !bit;
}

uint8_t button_get_state_bits(void) {
  uint8_t button_state = 0x00;
  for (int i = 0; i < OBELIX_BUTTON_NUM; ++i) {
    button_state |= (button_is_pressed(i) ? 0x01 : 0x00) << i;
  }
  return button_state;
}

void button_init(void) {
  for (int i = 0; i < OBELIX_BUTTON_NUM; ++i) {
    gpio_input_init_pull_up_down(&BOARD_CONFIG_BUTTON.buttons[i].gpioi, BOARD_CONFIG_BUTTON.buttons[i].pull);
  }
}

bool button_selftest(void) {
  return button_get_state_bits() == 0;
}

void command_button_read(const char* button_id_str) {

}
