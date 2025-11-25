/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pebbleos/cron.h>

#include "clar_asserts.h"

static CronJob *s_job = NULL;

time_t cron_job_schedule(CronJob *job) {
  s_job = job;
  return 0;
}

bool cron_job_unschedule(CronJob *job) {
  s_job = NULL;
  return true;
}

void fake_cron_job_fire(void) {
  cl_assert(s_job != NULL);
  CronJob *job = s_job;
  s_job = NULL;
  job->cb(job, job->cb_data);
}

