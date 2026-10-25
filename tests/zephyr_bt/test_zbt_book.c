/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The phone's books on the watch (ctl/zbt_book.c): what a reading saves, zbt_book_count() and
// zbt_book_read() give back, on the real file system and settings files.

#include <string.h>

#include <pbl/services/filesystem/pfs.h>

#include <clar.h>
#include <fake_pbl_malloc.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fake_system_task.h>
#include <zbt_link.h>

/* What the control side's book saving uses of the rest of it -------------------------------*/

static struct zbt_status s_status;
static int s_next_calls;

struct zbt_status *zbt_ctl_status(void) {
  return &s_status;
}

void zbt_host_pbap_next(void) {
  s_next_calls++;
}

/* Helpers ----------------------------------------------------------------------------------*/

static struct zbt_book_entry prv_entry(int i) {
  struct zbt_book_entry e = {.kind = (uint8_t)(i % 3), .month = 10, .day = 21, .year = 2026};
  snprintf(e.name, sizeof(e.name), "Name %d", i);
  snprintf(e.number, sizeof(e.number), "0901234%04d", i);
  return e;
}

// A reading of @p book with @p count entries, @p per_packet a packet, as the host hands them.
static void prv_read_book(enum zbt_book book, int count, int per_packet, bool ok) {
  cl_assert(zbt_ctl_pbap_acquire() != NULL);
  zbt_ctl_book_start(book);
  int i = 0;
  do {
    for (int n = 0; n < per_packet && i < count; n++, i++) {
      const struct zbt_book_entry e = prv_entry(i);
      cl_assert(zbt_ctl_book_entry(&e));
    }
    const bool end = i >= count;
    zbt_ctl_book_hand_over(end, end ? ok : true);
    fake_system_task_callbacks_invoke_pending();
  } while (i < count);
  zbt_ctl_pbap_release();
  fake_system_task_callbacks_invoke_pending();
}

/* Tests ------------------------------------------------------------------------------------*/

void test_zbt_book__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  memset(&s_status, 0, sizeof(s_status));
  s_next_calls = 0;
}

void test_zbt_book__cleanup(void) {
  zbt_book_host_stopped();
  fake_system_task_callbacks_cleanup();
}

void test_zbt_book__none_before_a_reading(void) {
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_CONTACTS), 0);
  struct zbt_book_entry e;
  cl_assert_equal_i(zbt_book_read(ZBT_BOOK_CONTACTS, 0, &e, 1), 0);
}

void test_zbt_book__a_reading_is_read_back(void) {
  prv_read_book(ZBT_BOOK_CONTACTS, 66, 3, true);
  cl_assert_equal_i(s_status.pbap_result[ZBT_BOOK_CONTACTS], 0);
  cl_assert_equal_i(s_status.pbap_entries[ZBT_BOOK_CONTACTS], 66);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_CONTACTS), 66);

  struct zbt_book_entry got[8];
  cl_assert_equal_i(zbt_book_read(ZBT_BOOK_CONTACTS, 60, got, 8), 6);
  const struct zbt_book_entry want = prv_entry(63);
  cl_assert_equal_s(got[3].name, want.name);
  cl_assert_equal_s(got[3].number, want.number);
}

void test_zbt_book__books_apart(void) {
  prv_read_book(ZBT_BOOK_CONTACTS, 66, 3, true);
  prv_read_book(ZBT_BOOK_HISTORY, 50, 4, true);
  prv_read_book(ZBT_BOOK_FAVORITES, 0, 1, true);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_CONTACTS), 66);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_HISTORY), 50);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_FAVORITES), 0);
}

void test_zbt_book__a_new_reading_replaces_the_book(void) {
  prv_read_book(ZBT_BOOK_HISTORY, 50, 4, true);
  prv_read_book(ZBT_BOOK_HISTORY, 7, 4, true);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_HISTORY), 7);
  prv_read_book(ZBT_BOOK_HISTORY, 9, 4, true);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_HISTORY), 9);
}

void test_zbt_book__a_failed_reading_keeps_the_last(void) {
  prv_read_book(ZBT_BOOK_CONTACTS, 20, 3, true);
  s_status.pbap_result[ZBT_BOOK_CONTACTS] = -110;
  prv_read_book(ZBT_BOOK_CONTACTS, 5, 3, false);
  cl_assert_equal_i(zbt_book_count(ZBT_BOOK_CONTACTS), 20);
}
