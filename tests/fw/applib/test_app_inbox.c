/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/app_inbox.h"
#include "kernel/events.h"
#include "services/normal/app_inbox_service.h"
#include "util/list.h"

extern bool app_inbox_service_has_inbox_for_tag(AppInboxServiceTag tag);
extern bool app_inbox_service_has_inbox_for_storage(uint8_t *storage);
extern bool app_inbox_service_is_being_written_for_tag(AppInboxServiceTag tag);
extern size_t app_inbox_service_num_failed_for_tag(AppInboxServiceTag tag);
extern size_t app_inbox_service_num_success_for_tag(AppInboxServiceTag tag);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Fakes & Stubs

#include "fake_kernel_malloc.h"
#include "fake_pebble_tasks.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_syscall_internal.h"

#define BUFFER_SIZE (32)
#define NOT_PERMITTED_MSG_HANDLER ((AppInboxMessageHandler)~0)
#define NOT_PERMITTED_DROP_HANDLER ((AppInboxDroppedHandler)~0)
#define TEST_TARGET_TASK (PebbleTask_App)

typedef struct {
  ListNode node;
  PebbleEvent event;
} EventNode;

static EventNode *s_event_head;
static bool s_can_send_event;

bool process_manager_send_event_to_process(PebbleTask task, PebbleEvent* e) {
  cl_assert_equal_i(PEBBLE_CALLBACK_EVENT, e->type);
  cl_assert_equal_i(task, TEST_TARGET_TASK);
  if (s_can_send_event) {
    EventNode *node = (EventNode *)malloc(sizeof(EventNode));
    *node = (const EventNode) {
      .event = *e,
    };
    s_event_head = (EventNode *)list_prepend((ListNode *)s_event_head, (ListNode *)node);
  }
  return s_can_send_event;
}

static void prv_process_callback_events_alt(bool should_execute_callback) {
  EventNode *node = s_event_head;
  while (node) {
    EventNode *next = (EventNode *) node->node.next;
    if (should_execute_callback) {
      node->event.callback.callback(node->event.callback.data);
    }
    free(node);
    node = next;
  }
  s_event_head = NULL;
}

static void prv_process_callback_events(void) {
  prv_process_callback_events_alt(true /* should_execute_callback */);
}

static void prv_cleanup_callback_events(void) {
  prv_process_callback_events_alt(false /* should_execute_callback */);
}

#define assert_num_callback_events(num) \
  cl_assert_equal_i(list_count((ListNode *)s_event_head), num);

////////////////////////////////////////////////////////////////////////////////////////////////////
// Inbox Service Stubs

void app_message_receiver_message_handler(const uint8_t *data, size_t length,
                                          AppInboxConsumerInfo *consumer_info) {
}

void app_message_receiver_dropped_handler(uint32_t num_dropped_messages) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test Inbox Service Handlers

#define TEST_ARRAY_SIZE (4)

static int s_message_idx;
static struct {
  uint8_t data[BUFFER_SIZE];
  size_t length;
} s_messages[TEST_ARRAY_SIZE];

static int s_num_messages_to_consume_from_handler;

void test_message_handler(const uint8_t *data, size_t length, AppInboxConsumerInfo *consumer_info) {
  cl_assert(s_message_idx < TEST_ARRAY_SIZE);
  s_messages[s_message_idx].length = length;
  memcpy(s_messages[s_message_idx].data, data, length);
  ++s_message_idx;
  if (s_num_messages_to_consume_from_handler--) {
    app_inbox_consume(consumer_info);
  }
}

#define assert_message(idx, dd, ll) \
{ \
  cl_assert(idx <= s_message_idx); \
  cl_assert_equal_i(ll, s_messages[idx].length); \
  cl_assert_equal_m(dd, s_messages[idx].data, ll); \
}

#define assert_num_message_callbacks(num_cbs) \
{ \
  cl_assert_equal_i(num_cbs, s_message_idx); \
}

static int s_dropped_idx;
static uint32_t s_dropped_messages[TEST_ARRAY_SIZE];

void test_dropped_handler(uint32_t num_dropped_messages) {
  cl_assert(s_dropped_idx < TEST_ARRAY_SIZE);
  s_dropped_messages[s_dropped_idx++] = num_dropped_messages;
}

#define assert_dropped(idx, num) \
{ \
  cl_assert(idx <= s_dropped_idx); \
  cl_assert_equal_i(num, s_dropped_messages[idx]); \
}

#define assert_num_dropped_callbacks(num_cbs) \
{ \
  cl_assert_equal_i(num_cbs, s_dropped_idx); \
}

void test_alt_message_handler(const uint8_t *data, size_t length,
                              AppInboxConsumerInfo *consumer_info) {
  cl_assert(false);
}

void test_alt_dropped_handler(uint32_t num_dropped_messages) {
  cl_assert(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

void test_app_inbox__initialize(void) {
  fake_kernel_malloc_init();
  fake_kernel_malloc_enable_stats(true);
  stub_pebble_tasks_set_current(TEST_TARGET_TASK);
  s_num_messages_to_consume_from_handler = 0;
  s_can_send_event = true;
  s_dropped_idx = 0;
  memset(s_dropped_messages, 0, sizeof(s_dropped_messages));
  s_message_idx = 0;
  memset(s_messages, 0, sizeof(s_messages));
}

void test_app_inbox__cleanup(void) {
  app_inbox_service_unregister_all();
  fake_kernel_malloc_deinit();
  prv_cleanup_callback_events();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// app_inbox_create_and_register

void test_app_inbox__app_inbox_create_and_register_zero_buffer_size(void) {
  void *result = app_inbox_create_and_register(0, 1, test_message_handler, test_dropped_handler);
  cl_assert_equal_p(result, NULL);
}

void test_app_inbox__app_inbox_create_and_register_zero_min_num_messages(void) {
  void *result = app_inbox_create_and_register(BUFFER_SIZE, 0,
                                               test_message_handler, test_dropped_handler);
  cl_assert_equal_p(result, NULL);
}

void test_app_inbox__app_inbox_create_and_register_null_message_handler(void) {
  void *result = app_inbox_create_and_register(BUFFER_SIZE, 1, NULL, test_dropped_handler);
  cl_assert_equal_p(result, NULL);
}

void test_app_inbox__app_inbox_create_and_register_oom(void) {
  // FIXME: No support for OOM simulation in applib_.. stub/fake
  return;
  void *result = app_inbox_create_and_register(BUFFER_SIZE, 1,
                                               test_message_handler,
                                               test_dropped_handler);
  cl_assert_equal_p(result, NULL);
}

void test_app_inbox__app_inbox_create_and_register_msg_handler_not_permitted(void) {
  // The syscall_failed() fake will trigger passert:
  cl_assert_passert(app_inbox_create_and_register(BUFFER_SIZE, 1,
                                                  NOT_PERMITTED_MSG_HANDLER,
                                                  test_dropped_handler));
}

void test_app_inbox__app_inbox_create_and_register_drop_handler_not_permitted(void) {
  // The syscall_failed() fake will trigger passert:
  cl_assert_passert(app_inbox_create_and_register(BUFFER_SIZE, 1,
                                                  test_message_handler,
                                                  NOT_PERMITTED_DROP_HANDLER));
}

void test_app_inbox__app_inbox_create_and_register_happy_case(void) {
  void *result = app_inbox_create_and_register(BUFFER_SIZE, 1,
                                               test_message_handler, test_dropped_handler);
  cl_assert(result != NULL);
  cl_assert_equal_b(true, app_inbox_service_has_inbox_for_tag(AppInboxServiceTagUnitTest));
}




void test_app_inbox__app_inbox_create_and_register_kernel_oom(void) {
  fake_kernel_malloc_set_largest_free_block(0);
  void *result = app_inbox_create_and_register(BUFFER_SIZE, 1,
                                               test_message_handler,
                                               test_dropped_handler);
  cl_assert_equal_p(result, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// app_inbox_service_register

void test_app_inbox__app_inbox_create_and_register_storage_already_associated(void) {
  bool success;
  uint8_t storage[BUFFER_SIZE];

  success = app_inbox_service_register(storage, sizeof(storage),
                                       test_message_handler, test_dropped_handler,
                                       AppInboxServiceTagUnitTest);
  cl_assert_equal_b(success, true);
  cl_assert_equal_b(true, app_inbox_service_has_inbox_for_storage(storage));
  cl_assert_equal_b(true, app_inbox_service_has_inbox_for_tag(AppInboxServiceTagUnitTest));

  fake_kernel_malloc_mark();
  success = app_inbox_service_register(storage, sizeof(storage),
                                       test_alt_message_handler, test_alt_dropped_handler,
                                       AppInboxServiceTagUnitTestAlt);
  cl_assert_equal_b(success, false);
  cl_assert_equal_b(false, app_inbox_service_has_inbox_for_tag(AppInboxServiceTagUnitTestAlt));
  fake_kernel_malloc_mark_assert_equal();
}

void test_app_inbox__app_inbox_create_and_register_tag_already_associated(void) {
  bool success;

  uint8_t storage[BUFFER_SIZE];
  success = app_inbox_service_register(storage, sizeof(storage),
                                       test_message_handler, test_dropped_handler,
                                       AppInboxServiceTagUnitTest);
  cl_assert_equal_b(success, true);

  fake_kernel_malloc_mark();
  uint8_t storage_alt[BUFFER_SIZE];
  success = app_inbox_service_register(storage_alt, sizeof(storage_alt),
                                       test_alt_message_handler, test_alt_dropped_handler,
                                       AppInboxServiceTagUnitTest /* same tag! */);
  cl_assert_equal_b(success, false);
  cl_assert_equal_b(false, app_inbox_service_has_inbox_for_storage(storage_alt));
  fake_kernel_malloc_mark_assert_equal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// app_inbox_service_begin

static void *s_writer = (void *) 0xaabbccdd;
static void *s_inbox;

static void prv_create_test_inbox(void) {
  s_inbox = app_inbox_create_and_register(BUFFER_SIZE, 1,
                                          test_message_handler, test_dropped_handler);
  cl_assert(s_inbox != NULL);
}

void test_app_inbox__app_inbox_service_begin_null_writer(void) {
  prv_create_test_inbox();
  cl_assert_equal_b(false, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                   BUFFER_SIZE, NULL));
  cl_assert_equal_b(false, app_inbox_service_is_being_written_for_tag(AppInboxServiceTagUnitTest));
}

void test_app_inbox__app_inbox_service_begin_no_inbox(void) {
  cl_assert_equal_b(false, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                   BUFFER_SIZE, s_writer));
  cl_assert_equal_b(false, app_inbox_service_is_being_written_for_tag(AppInboxServiceTagUnitTest));
}

void test_app_inbox__app_inbox_service_begin_already_being_written(void) {
  prv_create_test_inbox();
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  BUFFER_SIZE, s_writer));
  cl_assert_equal_b(true, app_inbox_service_is_being_written_for_tag(AppInboxServiceTagUnitTest));

  // Call ...begin() again:
  cl_assert_equal_b(false, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                   BUFFER_SIZE, s_writer));
  cl_assert_equal_b(true, app_inbox_service_is_being_written_for_tag(AppInboxServiceTagUnitTest));
  cl_assert_equal_i(1, app_inbox_service_num_failed_for_tag(AppInboxServiceTagUnitTest));
}

void test_app_inbox__app_inbox_service_begin_not_enough_storage_space(void) {
  prv_create_test_inbox();
  cl_assert_equal_b(false, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                   BUFFER_SIZE + 1, s_writer));
  cl_assert_equal_b(false, app_inbox_service_is_being_written_for_tag(AppInboxServiceTagUnitTest));
  cl_assert_equal_i(1, app_inbox_service_num_failed_for_tag(AppInboxServiceTagUnitTest));

  // Drop should be reported immediately (not after the next write finishes):
  prv_process_callback_events();
  assert_dropped(0, 1);
}

void test_app_inbox__app_inbox_service_begin_happy_case(void) {
  prv_create_test_inbox();
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  BUFFER_SIZE, s_writer));
  cl_assert_equal_b(true, app_inbox_service_is_being_written_for_tag(AppInboxServiceTagUnitTest));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// app_inbox_service_write / app_inbox_service_end

static const uint8_t s_test_data[] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09,
  0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
  0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
};

static void prv_create_test_inbox_and_begin_write(void) {
  prv_create_test_inbox();
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  BUFFER_SIZE, s_writer));
}

void test_app_inbox__app_inbox_service_write_inbox_closed_in_mean_time(void) {
  prv_create_test_inbox_and_begin_write();
  app_inbox_destroy_and_deregister(s_inbox);

  cl_assert_equal_b(false, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                   s_test_data, BUFFER_SIZE));
}

void test_app_inbox__app_inbox_service_write_not_enough_space(void) {
  prv_create_test_inbox_and_begin_write();
  cl_assert_equal_b(false, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                   s_test_data, BUFFER_SIZE + 1));

  // A continuation should also fail, even though there is enough space for it:
  cl_assert_equal_b(false, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                   s_test_data, 1));

  // After ending the write, expect num_failed to be incremented by one:
  app_inbox_service_end(AppInboxServiceTagUnitTest);
  cl_assert_equal_i(1, app_inbox_service_num_failed_for_tag(AppInboxServiceTagUnitTest));
  cl_assert_equal_i(0, app_inbox_service_num_success_for_tag(AppInboxServiceTagUnitTest));

  prv_process_callback_events();
  assert_num_dropped_callbacks(1);
  assert_num_message_callbacks(0);
}

void test_app_inbox__app_inbox_service_write_happy_case(void) {
  prv_create_test_inbox_and_begin_write();
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                  s_test_data, BUFFER_SIZE));
  // After ending the write, expect num_success to be incremented by one:
  app_inbox_service_end(AppInboxServiceTagUnitTest);
  cl_assert_equal_i(1, app_inbox_service_num_success_for_tag(AppInboxServiceTagUnitTest));
  cl_assert_equal_i(0, app_inbox_service_num_failed_for_tag(AppInboxServiceTagUnitTest));

  prv_process_callback_events();
  assert_message(0, s_test_data, BUFFER_SIZE);
  assert_num_message_callbacks(1);
  assert_num_dropped_callbacks(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// app_inbox_service_cancel

void test_app_inbox__app_inbox_service_cancel(void) {
  prv_create_test_inbox_and_begin_write();

  // Start writing a message that occupies the complete buffer, then cancel it:
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                  s_test_data, BUFFER_SIZE));
  app_inbox_service_cancel(AppInboxServiceTagUnitTest);

  // No events expected:
  assert_num_callback_events(0);

  // The buffer should be completely available again:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  BUFFER_SIZE, s_writer));
}

void test_app_inbox__app_inbox_service_cancel_non_existing_inbox(void) {
  app_inbox_service_cancel(AppInboxServiceTagUnitTest);

  // No events expected:
  assert_num_callback_events(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Consuming writes

void test_app_inbox__multiple_writes_while_consuming(void) {
  prv_create_test_inbox_and_begin_write();

  // Message 1:
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data, 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // Message 2:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest, 1, s_writer));
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data, 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // No space:
  cl_assert_equal_b(false, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                   BUFFER_SIZE + 1, s_writer));
  // Shouldn't call ..._end() here because ..._begin() failed.

  // Message 3:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest, 1, s_writer));
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data, 1));
  // ... still writing when event gets processed below

  // Only one callback event scheduled:
  assert_num_callback_events(1);

  prv_process_callback_events();
  assert_num_callback_events(0);

  // Expect 2 message callbacks and 1 drop callback:
  assert_num_message_callbacks(2);
  assert_message(0, s_test_data, 1);
  assert_message(1, s_test_data, 1);

  assert_num_dropped_callbacks(1);
  assert_dropped(0, 1);

  // Finish message 3, should be able to write (BUFFER_SIZE - 1) again,
  // because the message 1 and 2 are consumed now:
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data + 1,
                                                  BUFFER_SIZE - 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // One callback event scheduled:
  assert_num_callback_events(1);

  prv_process_callback_events();
  assert_num_callback_events(0);

  // Expect 3rd message callbacks and still 1 drop callback (same as before):
  assert_num_message_callbacks(3);
  assert_message(2, s_test_data, BUFFER_SIZE);

  assert_num_dropped_callbacks(1);
}


void test_app_inbox__multiple_writes_consume_from_message_handler(void) {
  prv_create_test_inbox_and_begin_write();

  // Message 1:
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data, 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // Message 2:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest, 1, s_writer));
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data, 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // Only one callback event scheduled:
  assert_num_callback_events(1);

  s_num_messages_to_consume_from_handler = 1;

  prv_process_callback_events();
  assert_num_callback_events(0);

  // Expect 2 message callbacks and 1 drop callback:
  assert_num_message_callbacks(2);
  assert_message(0, s_test_data, 1);
  assert_message(1, s_test_data, 1);

  // Should be able to write (BUFFER_SIZE) again,
  // because the message 1 and 2 are consumed now:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  BUFFER_SIZE, s_writer));
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data,
                                                  BUFFER_SIZE));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // One callback event scheduled:
  assert_num_callback_events(1);

  prv_process_callback_events();
  assert_num_callback_events(0);
}

void test_app_inbox__consume_inbox_closed_in_mean_time(void) {
  prv_create_test_inbox_and_begin_write();
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest, s_test_data, 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  cl_assert_equal_i(1, app_inbox_destroy_and_deregister(s_inbox));

  assert_num_callback_events(1);
  prv_process_callback_events();

  assert_num_dropped_callbacks(0);
  assert_num_message_callbacks(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// app_inbox_destroy_and_deregister / app_inbox_service_unregister_by_storage

void test_app_inbox__app_inbox_destroy_and_deregister_cleans_up_kernel_heap(void) {
  fake_kernel_malloc_mark();
  void *result = app_inbox_create_and_register(BUFFER_SIZE, 1,
                                               test_message_handler, test_dropped_handler);
  cl_assert_equal_i(app_inbox_destroy_and_deregister(result), 0);
  fake_kernel_malloc_mark_assert_equal();
}

void test_app_inbox__app_inbox_destroy_and_deregister_cleans_up_app_heap(void) {
  // TODO: No allocation tracking ability in applib_... stub/fake :(
}

void test_app_inbox__app_inbox_service_end_inbox_closed_in_mean_time(void) {
  prv_create_test_inbox_and_begin_write();
  // Expect to return 1, because one message is being dropped, the currently written one:
  cl_assert_equal_i(1, app_inbox_destroy_and_deregister(s_inbox));
  cl_assert_equal_b(false, app_inbox_service_end(AppInboxServiceTagUnitTest));
}

void test_app_inbox__app_inbox_service_end_inbox_closed_in_mean_time_with_pending_success(void) {
  prv_create_test_inbox_and_begin_write();
  // One message:
  cl_assert_equal_b(true, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                  s_test_data, 1));
  cl_assert_equal_b(true, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // Begin another one:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  1, s_writer));

  // Expect to return 2, because two messages are being dropped, the successful one that was not
  // yet processed and the currently written one:
  cl_assert_equal_i(2, app_inbox_destroy_and_deregister(s_inbox));
  cl_assert_equal_b(false, app_inbox_service_end(AppInboxServiceTagUnitTest));
}

void test_app_inbox__app_inbox_service_end_inbox_closed_in_mean_time_with_pending_failure(void) {
  prv_create_test_inbox_and_begin_write();
  // One message, too large, so it should get dropped:
  cl_assert_equal_b(false, app_inbox_service_write(AppInboxServiceTagUnitTest,
                                                   s_test_data, BUFFER_SIZE + 1));
  cl_assert_equal_b(false, app_inbox_service_end(AppInboxServiceTagUnitTest));

  // Begin another one:
  cl_assert_equal_b(true, app_inbox_service_begin(AppInboxServiceTagUnitTest,
                                                  1, s_writer));

  // Expect to return 2, because two messages are being dropped, the failed one that was not
  // yet processed and the currently written one:
  cl_assert_equal_i(2, app_inbox_destroy_and_deregister(s_inbox));
  cl_assert_equal_b(false, app_inbox_service_end(AppInboxServiceTagUnitTest));
}

void test_app_inbox__app_inbox_service_unregister_by_storage_unknown_storage(void) {
  uint8_t storage[BUFFER_SIZE];
  cl_assert_equal_i(app_inbox_service_unregister_by_storage(storage), 0);
}
