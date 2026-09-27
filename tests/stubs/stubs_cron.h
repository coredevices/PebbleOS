/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/cron/cron.h>

time_t pbl_cron_job_get_execute_time(const struct pbl_cron_job *job) {
  return 0;
}

time_t pbl_cron_job_get_execute_time_from_epoch(const struct pbl_cron_job *job,
                                                time_t local_epoch) {
  return 0;
}

time_t pbl_cron_job_schedule(struct pbl_cron_job *job) {
  return 0;
}

bool pbl_cron_job_unschedule(struct pbl_cron_job *job) {
  return true;
}
