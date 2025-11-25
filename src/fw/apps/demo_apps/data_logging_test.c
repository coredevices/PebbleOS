/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "data_logging_test.h"

#include "system/logging.h"
#include "services/common/comm_session/session.h"
#include "services/normal/data_logging/data_logging_service.h"
#include "services/normal/data_logging/dls_private.h"
#include "applib/app.h"
#include "applib/data_logging.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "applib/ui/text_layer.h"
#include "applib/app_timer.h"
#include "applib/app_logging.h"

#include <stdio.h>
#include <string.h>

/*
 *  Incremental STM CRC32 implemented in software
 */

#define CRC_POLY 0x04C11DB7

static uint32_t crc_init(void) {
  return 0xffffffff;
}

static uint32_t crc_update(uint32_t crc, const uint8_t *data, uint32_t length) {
  const uint8_t num_remainder_bytes = length % 4;
  uint32_t num_whole_word_bytes = length / 4;

  while (num_whole_word_bytes--) {
    crc = crc ^ *((uint32_t*)data);
    for(int bit = 0; bit < 32; ++bit) {
      if ((crc & 0x80000000) != 0) {
        crc = (crc << 1) ^ CRC_POLY;
      } else {
        crc = (crc << 1);
      }
    }
    data += 4;
  }

  if (num_remainder_bytes) {
    uint32_t last_word = 0;
    for (unsigned int i = 0; i < num_remainder_bytes; ++i) {
      last_word = (last_word << 8) | data[i];
    }
    return crc_update(crc, (uint8_t*)&last_word, 4);
  }

  return crc & 0xffffffff;
}

/*
 *  Data Logging Test App
 */

struct DataLoggingInfo {
  TextLayer text_layer;
  char text[32];
  int counter;
  uint32_t crc;
  DataLoggingSessionRef logging_session;
  uint8_t item_size;
};

static struct {
  Window window;
  struct DataLoggingInfo info[3];
  TextLayer log_layer;
} s_data;

static const int s_chunk_size = 80;

static void log_moar_data(struct DataLoggingInfo *info) {
  uint8_t buf[s_chunk_size];

  for (int i = 0; i < s_chunk_size; i++) {
    buf[i] = (info->counter * s_chunk_size) + i;
  }

  info->crc = crc_update(info->crc, buf, s_chunk_size);
  data_logging_log(info->logging_session, buf, s_chunk_size / info->item_size);
  ++info->counter;
}

static void handle_timer(void *ck) {
  if (s_data.info[0].logging_session == NULL) {
    // Sessions closed.
    return;
  }
  struct DataLoggingInfo*info = ck;

  log_moar_data(info);

  const int num_chunks = 30;

  snprintf(info->text, sizeof(info->text), "%lu (%i) %i/%i", info->crc, info->counter * s_chunk_size, info->counter, num_chunks);
  text_layer_set_text(&info->text_layer, info->text);

  if (info->counter < num_chunks) {
    app_timer_register(1000 /* milliseconds */, handle_timer, info);
  } else {
    text_layer_set_text(&s_data.log_layer, "Done logging. Select to close.");
  }
}

static void close_sessions(void) {
  for (int i = 0; i < 3; ++i) {
    data_logging_finish(s_data.info[i].logging_session);
    s_data.info[i].logging_session = NULL;
  }
  text_layer_set_text(&s_data.log_layer, "Closed all logging sessions.");
}

static void start_logging(void) {
  const uint8_t item_size[] = {4, 2, 16};
  const DataLoggingItemType types[] = {DATA_LOGGING_INT, DATA_LOGGING_UINT, DATA_LOGGING_BYTE_ARRAY};

  for (int i = 0; i < 3; ++i) {
    text_layer_set_text(&s_data.info[i].text_layer, "Empty");
    s_data.info[i].item_size = item_size[i];
    s_data.info[i].logging_session = data_logging_create(i + 1, types[i], item_size[i], false);
  }

  // start timers
  app_timer_register(2000 /* milliseconds */, handle_timer, (void *) &s_data.info[0]);
  app_timer_register(1500 /* milliseconds */, handle_timer, (void *) &s_data.info[1]);
  app_timer_register(4500 /* milliseconds */, handle_timer, (void *) &s_data.info[2]);

  text_layer_set_text(&s_data.log_layer, "Logging...");
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_data.info[0].logging_session) {
    close_sessions();
  } else {
    start_logging();
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}


static void handle_deinit(void) {
  comm_session_set_responsiveness(comm_session_get_system_session(), BtConsumerApp,
                                  ResponseTimeMax, MAX_PERIOD_RUN_FOREVER);
}

static void handle_init(void) {
  dls_clear();

  memset(&s_data, 0, sizeof(s_data));
  // Init window
  window_init(&s_data.window, "Logging Demo");
  app_window_stack_push(&s_data.window, true /* Animated */);
  window_set_click_config_provider_with_context(&s_data.window, click_config_provider,
                                                &s_data.window);

  const GRect *bounds = &s_data.window.layer.bounds;

  for (int i = 0; i < 3; ++i) {
    s_data.info[i].crc = crc_init();
    text_layer_init(&s_data.info[i].text_layer, &GRect(0, i * 20, bounds->size.w, 20));
    layer_add_child(&s_data.window.layer, &s_data.info[i].text_layer.layer);
  }

  text_layer_init(&s_data.log_layer, &GRect(0, bounds->size.w / 2, bounds->size.w,
                                            bounds->size.w / 2));
  layer_add_child(&s_data.window.layer, &s_data.log_layer.layer);

  start_logging();

  comm_session_set_responsiveness(comm_session_get_system_session(), BtConsumerApp,
                                  ResponseTimeMax, MAX_PERIOD_RUN_FOREVER);
}

////////////////////
// App boilerplate

static void s_main(void) {
  handle_init();

  app_event_loop();

  handle_deinit();
}

const PebbleProcessMd* data_logging_test_get_info() {
  static const PebbleProcessMdSystem s_app_info = {
    // UUID: 01020304-0506-0708-0910-111213141516
    .common.uuid = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16},
    .common.main_func = &s_main,
    .name = "Data Logging Test"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
