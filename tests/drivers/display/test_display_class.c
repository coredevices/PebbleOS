/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/display.h>
#include <pbl/kernel/timer.h>

#include <clar.h>

// The fake display cannot abort, so its updates never time out
void pbl_timer_init(struct pbl_timer *timer, pbl_timer_fn_t fn) {
}

void pbl_timer_start(struct pbl_timer *timer, pbl_timeout_t delay) {
}

void pbl_timer_stop(struct pbl_timer *timer) {
}

#define WIDTH       16U
#define HEIGHT      100U
#define MAX_UPDATES 8

struct prv_update {
  uint16_t y0;
  uint16_t y1;
};

static struct prv_update s_updates[MAX_UPDATES];
static int s_num_updates;
// Update, counted from 1, that fails to start or ends with an error, or 0 for none
static int s_fail_start_at;
static int s_fail_status_at;

static uint8_t s_fb[(WIDTH / 8U) * HEIGHT];
static const struct pbl_display_buffer s_buf = {.data = s_fb, .pitch = WIDTH / 8U};

static int prv_init(const struct pbl_display *disp) {
  return 0;
}

static int prv_enable(const struct pbl_display *disp) {
  return 0;
}

static int prv_set_rotated(const struct pbl_display *disp, bool rotated) {
  return 0;
}

// Completes synchronously
static int prv_update(const struct pbl_display *disp, const struct pbl_display_buffer *buf,
                      uint16_t y0, uint16_t y1) {
  int n;

  cl_assert(s_num_updates < MAX_UPDATES);
  s_updates[s_num_updates++] = (struct prv_update){.y0 = y0, .y1 = y1};
  n = s_num_updates;

  if (n == s_fail_start_at) {
    return -EIO;
  }

  pbl_display_complete(disp, (n == s_fail_status_at) ? -ETIMEDOUT : 0);

  return 0;
}

static const struct pbl_display_ops s_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_enable,
  .set_rotated = prv_set_rotated,
  .update = prv_update,
};

PBL_DISPLAY_STATE_DEFINE(s_whole);
static const struct pbl_display s_whole = PBL_DISPLAY_INIT(s_whole, "whole", nullptr, &s_ops, WIDTH,
                                                           HEIGHT, PBL_DISPLAY_FORMAT_MONO, false);

PBL_DISPLAY_STATE_DEFINE(s_banded);
static const struct pbl_display s_banded =
    PBL_DISPLAY_INIT(s_banded, "banded", nullptr, &s_ops, WIDTH, HEIGHT, PBL_DISPLAY_FORMAT_MONO,
                     false, .max_update_rows = PBL_DISPLAY_MAX_UPDATE_ROWS(HEIGHT));

static void prv_assert_update(int idx, uint16_t y0, uint16_t y1) {
  cl_assert_equal_i(s_updates[idx].y0, y0);
  cl_assert_equal_i(s_updates[idx].y1, y1);
}

static int s_cb_status;
static int s_cb_count;

static void prv_done(int status, void *arg) {
  s_cb_status = status;
  s_cb_count++;
}

void test_display_class__initialize(void) {
  s_num_updates = 0;
  s_fail_start_at = 0;
  s_fail_status_at = 0;
  s_cb_status = 1;
  s_cb_count = 0;
  cl_assert_equal_i(pbl_device_init(&s_whole.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_banded.dev), 0);
}

void test_display_class__max_rows_from_the_class_option(void) {
  cl_assert_equal_i(PBL_DISPLAY_MAX_UPDATE_ROWS(HEIGHT), 24);
  cl_assert_equal_i(PBL_DISPLAY_MAX_UPDATE_ROWS(10), 10);
  cl_assert_equal_i(pbl_display_max_update_rows(&s_banded), 24);
  cl_assert_equal_i(pbl_display_max_update_rows(&s_whole), HEIGHT);
}

void test_display_class__update_takes_at_most_max_rows(void) {
  cl_assert_equal_i(pbl_display_update(&s_banded, &s_buf, 10, 33, prv_done, nullptr), 0);
  cl_assert_equal_i(s_cb_count, 1);
  cl_assert_equal_i(s_cb_status, 0);

  cl_assert_equal_i(pbl_display_update(&s_banded, &s_buf, 10, 34, prv_done, nullptr), -EINVAL);
  cl_assert_equal_i(s_cb_count, 1);
  cl_assert_equal_i(s_num_updates, 1);
  cl_assert(!pbl_display_update_in_progress(&s_banded));
}

void test_display_class__update_of_whole_panel(void) {
  cl_assert_equal_i(pbl_display_update(&s_whole, &s_buf, 0, HEIGHT - 1, prv_done, nullptr), 0);
  cl_assert_equal_i(s_num_updates, 1);
  prv_assert_update(0, 0, HEIGHT - 1);
}

void test_display_class__sync_sends_whole_panel_at_once(void) {
  cl_assert_equal_i(pbl_display_update_sync(&s_whole, &s_buf, 0, HEIGHT - 1), 0);
  cl_assert_equal_i(s_num_updates, 1);
  prv_assert_update(0, 0, HEIGHT - 1);
}

void test_display_class__sync_sends_in_bands(void) {
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 0, HEIGHT - 1), 0);
  cl_assert_equal_i(s_num_updates, 5);
  prv_assert_update(0, 0, 23);
  prv_assert_update(1, 24, 47);
  prv_assert_update(2, 48, 71);
  prv_assert_update(3, 72, 95);
  prv_assert_update(4, 96, 99);
}

void test_display_class__sync_bands_a_range(void) {
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 30, 60), 0);
  cl_assert_equal_i(s_num_updates, 2);
  prv_assert_update(0, 30, 53);
  prv_assert_update(1, 54, 60);

  s_num_updates = 0;
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 99, 99), 0);
  cl_assert_equal_i(s_num_updates, 1);
  prv_assert_update(0, 99, 99);
}

void test_display_class__sync_stops_at_a_band_failing_to_start(void) {
  s_fail_start_at = 2;
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 0, HEIGHT - 1), -EIO);
  cl_assert_equal_i(s_num_updates, 2);
  cl_assert(!pbl_display_update_in_progress(&s_banded));
}

void test_display_class__sync_stops_at_a_band_failing(void) {
  s_fail_status_at = 3;
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 0, HEIGHT - 1), -ETIMEDOUT);
  cl_assert_equal_i(s_num_updates, 3);
  cl_assert(!pbl_display_update_in_progress(&s_banded));
}

void test_display_class__sync_rejects_invalid_range(void) {
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 0, HEIGHT), -EINVAL);
  cl_assert_equal_i(pbl_display_update_sync(&s_banded, &s_buf, 50, 49), -EINVAL);
  cl_assert_equal_i(s_num_updates, 0);
}
