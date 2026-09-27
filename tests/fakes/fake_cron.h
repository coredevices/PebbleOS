/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/cron/cron.h>

#include "clar_asserts.h"

static struct pbl_cron_job *s_job = NULL;

time_t pbl_cron_job_schedule(struct pbl_cron_job *job) {
  s_job = job;
  return 0;
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
