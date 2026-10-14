/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/cron/cron.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK time_t pbl_cron_job_get_execute_time(const struct pbl_cron_job *job) {
  return 0;
}

PBL_WEAK time_t pbl_cron_job_get_execute_time_from_epoch(const struct pbl_cron_job *job,
                                                         time_t local_epoch) {
  return 0;
}

PBL_WEAK time_t pbl_cron_job_schedule(struct pbl_cron_job *job) {
  return 0;
}

PBL_WEAK bool pbl_cron_job_unschedule(struct pbl_cron_job *job) {
  return true;
}
