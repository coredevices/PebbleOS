/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ancs_phone_call.h"

#include "applib/graphics/utf8.h"
#include "kernel/events.h"
#include "services/common/regular_timer.h"
#include "services/normal/phone_call_util.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static RegularTimerInfo s_missed_call_timer_id;

static void prv_put_call_event(PhoneEventType type, uint32_t call_identifier,
                               PebblePhoneCaller *caller, bool ios_9) {
  PebbleEvent event = {
    .type = PEBBLE_PHONE_EVENT,
    .phone = {
      .type = type,
      .source = ios_9 ? PhoneCallSource_ANCS : PhoneCallSource_ANCS_Legacy,
      .call_identifier = call_identifier,
      .caller = caller,
    }
  };

  event_put(&event);
}

static void prv_strip_formatting_chars(char *text) {
  const size_t len = strlen(text);
  utf8_t *w_cursor = (uint8_t *)text;
  utf8_t *r_cursor = (uint8_t *)text;

  utf8_t *next;
  uint32_t codepoint;
  while (true) {
    codepoint = utf8_peek_codepoint(r_cursor, &next);
    if (codepoint == 0) {
      break;
    }

    if (!codepoint_is_formatting_indicator(codepoint)) {
      uint8_t c_len = utf8_copy_character(w_cursor, r_cursor, len);
      w_cursor += c_len;
    }

    r_cursor = next;
  }
  *w_cursor = '\0';
}

void ancs_phone_call_handle_incoming(uint32_t uid, ANCSProperty properties,
                                     ANCSAttribute **notif_attributes) {
  // This field holds the caller's name if the phone number belongs to a contact,
  // or the actual phone number if it does not belong to a contact
  const ANCSAttribute *caller_id = notif_attributes[FetchedNotifAttributeIndexTitle];

  char caller_id_str[caller_id->length + 1];
  pstring_pstring16_to_string(&caller_id->pstr, caller_id_str);
  prv_strip_formatting_chars(caller_id_str);
  PebblePhoneCaller *caller = phone_call_util_create_caller(caller_id_str, NULL);

  const bool ios_9 = (properties & ANCSProperty_iOS9);

  prv_put_call_event(PhoneEventType_Incoming, uid, caller, ios_9);
}

void ancs_phone_call_handle_removed(uint32_t uid, bool ios_9) {
  prv_put_call_event(PhoneEventType_Hide, uid, NULL, ios_9);
}

bool ancs_phone_call_should_ignore_missed_calls(void) {
  return regular_timer_is_scheduled(&s_missed_call_timer_id);
}

static void prv_handle_missed_call_timer_timeout(void *not_used) {
if (regular_timer_is_scheduled(&s_missed_call_timer_id)) {
    regular_timer_remove_callback(&s_missed_call_timer_id);
  }
}

void ancs_phone_call_temporarily_block_missed_calls(void) {
  const int BLOCK_MISS_CALL_TIME_S = 7;
  if (regular_timer_is_scheduled(&s_missed_call_timer_id)) {
    regular_timer_remove_callback(&s_missed_call_timer_id);
  }

  s_missed_call_timer_id = (const RegularTimerInfo) {
    .cb = prv_handle_missed_call_timer_timeout,
  };
  regular_timer_add_multisecond_callback(&s_missed_call_timer_id, BLOCK_MISS_CALL_TIME_S);
}
