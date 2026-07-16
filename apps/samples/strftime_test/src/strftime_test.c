/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pebble.h>

static Window *window;
static TextLayer *result_layer;

static struct tm good_data = {
  .tm_sec = 49,
  .tm_min = 4,
  .tm_hour = 11,
  .tm_mday = 5,
  .tm_mon = 4,
  .tm_year = 115,
  .tm_wday = 2,
  .tm_yday = 124,
  .tm_isdst = 1
};

static struct tm bad_data = {
  .tm_sec = 49756567,
  .tm_min = 49756567,
  .tm_hour = 49756567,
  .tm_mday = 49756567,
  .tm_mon = 49756567,
  .tm_year = 49756567,
  .tm_wday = 49756567,
  .tm_yday = 49756567,
  .tm_isdst = 49756567
};


static void prv_test_valid_data(void) {
  const int buf_size = 64;
  char buf[buf_size];
  int r;

  // Make sure the valid struct works as expected
  r = strftime(buf, buf_size, "%a", &good_data);
  if (r == 0 || strncmp(buf, "Tue", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"a\": %s", buf);
  }

  r = strftime(buf, buf_size, "%A", &good_data);
  if (r == 0 || strncmp(buf, "Tuesday", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"A\": %s", buf);
  }

  r = strftime(buf, buf_size, "%b", &good_data);
  if (r == 0 || strncmp(buf, "May", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"b\": %s", buf);
  }

  r = strftime(buf, buf_size, "%B", &good_data);
  if (r == 0 || strncmp(buf, "May", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"B\": %s", buf);
  }

  r = strftime(buf, buf_size, "%c", &good_data);
  if (r == 0 || strncmp(buf, "Tue May  5 11:04:49 2015", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"c\": %s", buf);
  }

  r = strftime(buf, buf_size, "%d", &good_data);
  if (r == 0 || strncmp(buf, "05", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"d\": %s", buf);
  }

  r = strftime(buf, buf_size, "%D", &good_data);
  if (r == 0 || strncmp(buf, "05/05/15", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"D\": %s", buf);
  }

  r = strftime(buf, buf_size, "%e", &good_data);
  if (r == 0 || strncmp(buf, " 5", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"e\": %s", buf);
  }

  r = strftime(buf, buf_size, "%F", &good_data);
  if (r == 0 || strncmp(buf, "2015-05-05", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"F\": %s", buf);
  }

  r = strftime(buf, buf_size, "%g", &good_data);
  if (r == 0 || strncmp(buf, "15", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"f\": %s", buf);
  }

  r = strftime(buf, buf_size, "%G", &good_data);
  if (r == 0 || strncmp(buf, "2015", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"G\": %s", buf);
  }

  r = strftime(buf, buf_size, "%h", &good_data);
  if (r == 0 || strncmp(buf, "May", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"h\": %s", buf);
  }

  r = strftime(buf, buf_size, "%H", &good_data);
  if (r == 0 || strncmp(buf, "11", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"H\": %s", buf);
  }

  r = strftime(buf, buf_size, "%I", &good_data);
  if (r == 0 || strncmp(buf, "11", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"I\": %s", buf);
  }

  r = strftime(buf, buf_size, "%j", &good_data);
  if (r == 0 || strncmp(buf, "125", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"j\": %s", buf);
  }

  r = strftime(buf, buf_size, "%m", &good_data);
  if (r == 0 || strncmp(buf, "05", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"m\": %s", buf);
  }

  r = strftime(buf, buf_size, "%M", &good_data);
  if (r == 0 || strncmp(buf, "04", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"M\": %s", buf);
  }

  r = strftime(buf, buf_size, "%p", &good_data);
  if (r == 0 || strncmp(buf, "AM", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"p\": %s", buf);
  }

  r = strftime(buf, buf_size, "%r", &good_data);
  if (r == 0 || strncmp(buf, "11:04:49 AM", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"r\": %s", buf);
  }

  r = strftime(buf, buf_size, "%R", &good_data);
  if (r == 0 || strncmp(buf, "11:04", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"R\": %s", buf);
  }

  r = strftime(buf, buf_size, "%S", &good_data);
  if (r == 0 || strncmp(buf, "49", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"S\": %s", buf);
  }

  r = strftime(buf, buf_size, "%T", &good_data);
  if (r == 0 || strncmp(buf, "11:04:49", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"T\": %s", buf);
  }

  r = strftime(buf, buf_size, "%u", &good_data);
  if (r == 0 || strncmp(buf, "2", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"u\": %s", buf);
  }

  r = strftime(buf, buf_size, "%U", &good_data);
  if (r == 0 || strncmp(buf, "18", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"U\": %s", buf);
  }

  r = strftime(buf, buf_size, "%V", &good_data);
  if (r == 0 || strncmp(buf, "19", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"V\": %s", buf);
  }

  r = strftime(buf, buf_size, "%w", &good_data);
  if (r == 0 || strncmp(buf, "2", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"w\": %s", buf);
  }

  r = strftime(buf, buf_size, "%W", &good_data);
  if (r == 0 || strncmp(buf, "18", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"W\": %s", buf);
  }

  r = strftime(buf, buf_size, "%x", &good_data);
  if (r == 0 || strncmp(buf, "05/05/15", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"x\": %s", buf);
  }

  r = strftime(buf, buf_size, "%X", &good_data);
  if (r == 0 || strncmp(buf, "11:04:49", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"X\": %s", buf);
  }

  r = strftime(buf, buf_size, "%y", &good_data);
  if (r == 0 || strncmp(buf, "15", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"y\": %s", buf);
  }

  r = strftime(buf, buf_size, "%Y", &good_data);
  if (r == 0 || strncmp(buf, "2015", buf_size) != 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"Y\": %s", buf);
  }

  // r = strftime(buf, buf_size, "%z", &good_data);
  // if (r == 0 || strncmp(buf, "", buf_size) != 0) {
  //   APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"z\": %s", buf);
  // }

  // r = strftime(buf, buf_size, "%Z", &good_data);
  // if (r == 0 || strncmp(buf, "", buf_size) != 0) {
  //   APP_LOG(APP_LOG_LEVEL_DEBUG, "Error with \"Z\": %s", buf);
  // }
}

static void prv_test_invalid_data(void) {
  const int buf_size = 64;
  char buf[buf_size];

  // Make sure the invalid structs don't crash us
  // These should all return 0, but many don't seem to be doing that
  strftime(buf, buf_size, "%a", &bad_data);

  strftime(buf, buf_size, "%A", &bad_data);

  strftime(buf, buf_size, "%b", &bad_data);

  strftime(buf, buf_size, "%B", &bad_data);

  strftime(buf, buf_size, "%c", &bad_data);

  strftime(buf, buf_size, "%d", &bad_data);

  strftime(buf, buf_size, "%D", &bad_data);

  strftime(buf, buf_size, "%e", &bad_data);

  strftime(buf, buf_size, "%F", &bad_data);

  strftime(buf, buf_size, "%g", &bad_data);

  strftime(buf, buf_size, "%G", &bad_data);

  strftime(buf, buf_size, "%h", &bad_data);

  strftime(buf, buf_size, "%H", &bad_data);

  strftime(buf, buf_size, "%I", &bad_data);

  strftime(buf, buf_size, "%j", &bad_data);

  strftime(buf, buf_size, "%m", &bad_data);

  strftime(buf, buf_size, "%M", &bad_data);

  strftime(buf, buf_size, "%p", &bad_data);

  strftime(buf, buf_size, "%r", &bad_data);

  strftime(buf, buf_size, "%R", &bad_data);

  strftime(buf, buf_size, "%S", &bad_data);

  strftime(buf, buf_size, "%T", &bad_data);

  strftime(buf, buf_size, "%u", &bad_data);

  strftime(buf, buf_size, "%U", &bad_data);

  strftime(buf, buf_size, "%V", &bad_data);

  strftime(buf, buf_size, "%w", &bad_data);

  strftime(buf, buf_size, "%W", &bad_data);

  strftime(buf, buf_size, "%x", &bad_data);

  strftime(buf, buf_size, "%X", &bad_data);

  strftime(buf, buf_size, "%y", &bad_data);

  strftime(buf, buf_size, "%Y", &bad_data);

  // strftime(buf, buf_size, "%z", &bad_data);

  // strftime(buf, buf_size, "%Z", &bad_data);
}

static void window_load(Window *window) {
  prv_test_valid_data();
  prv_test_invalid_data();


  Layer *window_layer = window_get_root_layer(window);
  result_layer = text_layer_create(GRect(0, 0, 144, 168));
  text_layer_set_text(result_layer, "strftime() test. Check the app logs for details");
  text_layer_set_font(result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(result_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(result_layer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
