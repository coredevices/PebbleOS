/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/data_logging/data_logging_service.h"
#include <pbl/drivers/rtc.h>
#include "pbl/util/build_id.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
#include "stubs_rtc.h"

void pbl_analytics__native_heartbeat(void);

const ElfExternalNote TINTIN_BUILD_ID = {
  .name_length = 4,
  .data_length = BUILD_ID_EXPECTED_LEN,
  .type = 3,
};

static int s_dls_create_count;
static DataLoggingSession *s_dls_create_result;
static int s_dls_log_count;

DataLoggingSession *dls_create(uint32_t tag, DataLoggingItemType item_type, uint16_t item_size,
                               bool buffered, bool resume, const Uuid *uuid) {
  s_dls_create_count++;
  return s_dls_create_result;
}

DataLoggingResult dls_log(DataLoggingSession *session, const void *data, uint32_t num_items) {
  cl_assert_equal_p(session, s_dls_create_result);
  s_dls_log_count++;
  return DATA_LOGGING_SUCCESS;
}

void test_analytics_native__initialize(void) {
  s_dls_create_count = 0;
  s_dls_create_result = NULL;
  s_dls_log_count = 0;
}

void test_analytics_native__session_unavailable_retries_next_heartbeat(void) {
  static uint8_t s_session_storage;

  pbl_analytics__native_heartbeat();
  cl_assert_equal_i(s_dls_create_count, 1);
  cl_assert_equal_i(s_dls_log_count, 0);

  s_dls_create_result = (DataLoggingSession *)&s_session_storage;
  pbl_analytics__native_heartbeat();
  cl_assert_equal_i(s_dls_create_count, 2);
  cl_assert_equal_i(s_dls_log_count, 1);

  pbl_analytics__native_heartbeat();
  cl_assert_equal_i(s_dls_create_count, 2);
  cl_assert_equal_i(s_dls_log_count, 2);
}
