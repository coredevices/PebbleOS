/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// time
static time_t s_time = 0;
static uint16_t s_millis = 0;
// TZ
static int32_t s_gmt_off = 0;
// DST
static int32_t s_dst_off = 0;
static time_t s_dst_start = 0;
static time_t s_dst_stop = 0;

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  if (tloc) {
    *tloc = s_time;
  }
  if (out_ms) {
    *out_ms = s_millis;
  }

  return 0;
}

int32_t time_get_gmtoffset(void) {
  return s_gmt_off;
}

int32_t time_get_dstoffset(void) {
  return s_dst_off;
}

bool time_get_isdst(time_t utc_time) {
  if ((s_dst_start == 0) || (s_dst_stop == 0)) {
    return false;
  }

  return ((s_dst_start <= utc_time) && (utc_time < s_dst_stop));
}

time_t time_utc_to_local(time_t utc_time) {
  int32_t dst_offset = (utc_time > s_dst_start && utc_time < s_dst_stop) ? s_dst_off : 0;
  return utc_time + s_gmt_off + dst_offset;
}

time_t time_local_to_utc(time_t local_time) {
  int32_t dst_offset = ((local_time + s_dst_off) > s_dst_start &&
                        (local_time + s_dst_off) < s_dst_stop
                       ) ? s_dst_off : 0;
  return (local_time - s_gmt_off) - dst_offset;
}

void fake_time_init(time_t initial_time, uint16_t initial_ms) {
  s_time = initial_time;
  s_millis = initial_ms;
}

void fake_time_set_dst(int32_t offset, int32_t start, int32_t stop) {
  s_dst_off = offset;
  s_dst_start = start;
  s_dst_stop = stop;
}

void fake_time_set_gmtoff(int32_t gmtoff) {
  s_gmt_off = gmtoff;
}


