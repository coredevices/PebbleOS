/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "console/prompt.h"
#include "drivers/accessory.h"
#include "services/common/system_task.h"
#include "system/logging.h"
#include "util/likely.h"
#include "util/math.h"

#include <stdint.h>
#include <string.h>

static void prv_command_response_callback(const char* response) {
  accessory_send_data((const uint8_t*) response, strlen(response));
  accessory_send_data((const uint8_t*) "\r\n", sizeof(char) * 2);
}

static void prv_display_prompt(void) {
  accessory_send_data((const uint8_t*) ">", sizeof(char));
}

static PromptContext s_prompt_context = {
  .response_callback = prv_command_response_callback,
  .command_complete_callback = prv_display_prompt,
};

static void prv_execute_command(void *data) {
  PromptContext *prompt_context = (PromptContext *)data;

  // Copy the command and append a NULL so we can print it for debugging purposes
  char buffer[40];
  size_t cropped_length = MIN(sizeof(buffer) - 1, prompt_context->write_index);
  memcpy(buffer, prompt_context->buffer, cropped_length);
  buffer[cropped_length] = 0;
  PBL_LOG(LOG_LEVEL_DEBUG, "Exec command <%s>", buffer);

  prompt_context_execute(prompt_context);
}

void accessory_mfg_mode_start(void) {
  #ifdef DISABLE_PROMPT
    return;
  #else
    prv_display_prompt();
  #endif
}

bool accessory_mfg_mode_handle_char(char c) {
  // Note: You're in an interrupt here, be careful
#if DISABLE_PROMPT
  return false;
#else
  if (UNLIKELY(prompt_command_is_executing())) {
    return false;
  }
  bool should_context_switch = false;

  if (LIKELY(c >= 0x20 && c < 127)) {
    prompt_context_append_char(&s_prompt_context, c);
  } else if (UNLIKELY(c == 0xd)) { // Enter key
    system_task_add_callback_from_isr(
        prv_execute_command, &s_prompt_context, &should_context_switch);
  } else if (UNLIKELY(c == 0x3)) {  // CTRL-C
    // FIXME: Clean this up so this logic doesn't need to be duplicated here
    s_prompt_context.write_index = 0;
    prv_display_prompt();
  }

  return should_context_switch;
#endif
}

