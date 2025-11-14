#include <stdlib.h>

#include "drivers/button.h"
#include "console/prompt.h"

bool button_is_pressed(ButtonId id) {
  return false;
}

uint8_t button_get_state_bits(void) {
  return 0;
}

void button_init(void) {
}

void button_set_rotated(bool rotated) {
  
}

bool button_selftest(void) {
  return true;
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
