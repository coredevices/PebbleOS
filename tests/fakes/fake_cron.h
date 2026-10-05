/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/cron/cron.h>

#include "clar_asserts.h"

static struct pbl_cron_job *s_job = NULL;
static time_t s_job_time;

time_t pbl_cron_job_schedule(struct pbl_cron_job *job) {
  s_job = job;
  return 0;
}

void pbl_cron_job_schedule_at(struct pbl_cron_job *job, time_t utc_time) {
  s_job = job;
  s_job_time = utc_time;
}

//! Fires the scheduled job if it is due at @p now.
bool fake_cron_job_fire_if_due(time_t now) {
  if (s_job == NULL || s_job_time > now) {
    return false;
  }
  struct pbl_cron_job *job = s_job;
  s_job = NULL;
  job->cb(job, job->cb_data);
  return true;
}

bool pbl_cron_job_unschedule(struct pbl_cron_job *job) {
  s_job = NULL;
  return true;
}

void fake_cron_job_fire(void) {
  cl_assert(s_job != NULL);
  struct pbl_cron_job *job = s_job;
  s_job = NULL;
  job->cb(job, job->cb_data);
}
