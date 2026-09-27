/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "pbl/util/list.h"

//! @file cron.h
//! Wall-clock based timer system. Designed for use in things such as alarms, calendar events, etc.
//! Properly handles DST, etc.

struct pbl_cron_job;

typedef void (*pbl_cron_job_cb_t)(struct pbl_cron_job *job, void *data);

//! Matches any possible value.
#define PBL_CRON_MINUTE_ANY (-1)
#define PBL_CRON_HOUR_ANY   (-1)
#define PBL_CRON_MDAY_ANY   (-1)
#define PBL_CRON_MONTH_ANY  (-1)

#define PBL_CRON_WDAY_SUNDAY    (1 << 0)
#define PBL_CRON_WDAY_MONDAY    (1 << 1)
#define PBL_CRON_WDAY_TUESDAY   (1 << 2)
#define PBL_CRON_WDAY_WEDNESDAY (1 << 3)
#define PBL_CRON_WDAY_THURSDAY  (1 << 4)
#define PBL_CRON_WDAY_FRIDAY    (1 << 5)
#define PBL_CRON_WDAY_SATURDAY  (1 << 6)

#define PBL_CRON_WDAY_WEEKDAYS                                              \
  (PBL_CRON_WDAY_MONDAY | PBL_CRON_WDAY_TUESDAY | PBL_CRON_WDAY_WEDNESDAY | \
   PBL_CRON_WDAY_THURSDAY | PBL_CRON_WDAY_FRIDAY)
#define PBL_CRON_WDAY_WEEKENDS (PBL_CRON_WDAY_SUNDAY | PBL_CRON_WDAY_SATURDAY)
#define PBL_CRON_WDAY_ANY      (PBL_CRON_WDAY_WEEKENDS | PBL_CRON_WDAY_WEEKDAYS)

struct pbl_cron_job {
  //! internal, no touchy
  ListNode list_node;

  //! Cached execution timestamp in UTC.
  //! This is set by `pbl_cron_job_schedule`, and is required to never be changed once the job has
  //! been added.
  time_t cached_execute_time;

  //! Callback that is called when the job fires.
  pbl_cron_job_cb_t cb;
  void *cb_data;

  //! Occasionally, the system gets a clock change event for various reasons:
  //!  - User changed time-zones or a DST transition happened
  //!  - User changed the time
  //!  - Phone sent the current time and was different from ours, so we took theirs.
  //! In the first case, the cron job's execute time will always be recalculated.
  //! In the other two, we see if the time difference from the old time is >= this.
  //! If it is, then we'll recalculate. Otherwise, we leave the calculated time alone.
  //! In this way, 0 will always recalculate, and UINT32_MAX will never recalculate.
  //!
  //! Recalculating would essentially mean that a job that was "skipped over" will not fire until
  //! the next match. If recalculation is not done, but the job was skipped over, it will fire
  //! instantly.
  //!
  //! This value is specified in seconds.
  uint32_t clock_change_tolerance;

  int8_t minute; //!< 0-59, or PBL_CRON_MINUTE_ANY
  int8_t hour;   //!< 0-23, or PBL_CRON_HOUR_ANY
  int8_t mday;   //!< 0-30, or PBL_CRON_MDAY_ANY
  int8_t month;  //!< 0-11, or PBL_CRON_MONTH_ANY

  //! Seconds to offset the cron execution time applied after regular cron job time calculation.
  //! For example, a cron scheduled for Monday at 0:15 with an offset of negative 30min will fire
  //! on Sunday at 23:45.
  int32_t offset_seconds;

  union {
    uint8_t flags;

    struct {
      //! This should be any combination of PBL_CRON_WDAY_*. If zero, acts like PBL_CRON_WDAY_ANY.
      uint8_t wday : 7;

      //! If this flag is set, the resulting execution time may be equal to the local epoch.
      //! Having it set could be used for some event that must happen at the specified time even if
      //! that time is right now.
      bool may_be_instant : 1;
    };
  };
};

//! Initialize the cron subsystem.
void pbl_cron_init(void);

//! Adjust all cron jobs, as the wall clock has changed.
//! @param utc_time_delta seconds the UTC time moved by.
//! @param gmt_offset_delta seconds the GMT offset moved by; non-zero forces recalculation.
//! @param dst_changed whether the DST state changed; true forces recalculation.
void pbl_cron_handle_clock_change(int32_t utc_time_delta, int32_t gmt_offset_delta,
                                  bool dst_changed);

//! Add a cron job. This will make the subsystem hold a reference to the specified job, so it must
//! not leave scope or be destroyed until it is unscheduled.
//! The job only gets scheduled once. For re-scheduling, you can call this on the job again.
//! @param job pointer to the job to be scheduled.
//! @returns time_t for when the job is destined to go off.
time_t pbl_cron_job_schedule(struct pbl_cron_job *job);

//! Schedule a cron job to run after another cron job.
//! This will make the subsystem hold a reference to the new job, so it must
//! not leave scope or be destroyed until it is unscheduled.
//! @param job pointer to the job after which we want our job to run. job must be scheduled.
//! @param new_job pointer to the job to be scheduled. new_job must be unscheduled.
//! @returns time_t for when the job is destined to go off.
//! @note This API makes no guarantee that the two jobs will be scheduled back to back,
//! only that new_job will have the same scheduled time as job and that it will trigger
//! strictly after job.
time_t pbl_cron_job_schedule_after(struct pbl_cron_job *job, struct pbl_cron_job *new_job);

//! Remove a scheduled cron job.
//! @param job pointer to the job to be unscheduled.
//! @return true if the job was successfully removed (false may indicate no job was
//!  scheduled at all or the cb is currently executing)
bool pbl_cron_job_unschedule(struct pbl_cron_job *job);

//! Check if a cron job is scheduled.
//! @param job pointer to the job to be checked for being scheduled.
//! @returns true if scheduled or pending deletion, false otherwise
bool pbl_cron_job_is_scheduled(struct pbl_cron_job *job);

//! Calculate cron job's destined execution time, from the current time.
//! @param job pointer to the job to get the execution time for.
//! @returns time_t for when the job is destined to go off.
time_t pbl_cron_job_get_execute_time(const struct pbl_cron_job *job);

//! Calculate cron job's destined execution time if it were scheduled at the given time.
//! @param job pointer to the job to get the execution time for.
//! @param local_epoch the epoch for getting the job's execution time.
//! @returns time_t for when the job is destined to go off.
time_t pbl_cron_job_get_execute_time_from_epoch(const struct pbl_cron_job *job, time_t local_epoch);

#if UNITTEST
//! Remove all jobs.
void pbl_cron_clear_all_jobs(void);

//! Clean up the cron subsystem.
void pbl_cron_deinit(void);

//! The number of registered cron jobs.
uint32_t pbl_cron_get_job_count(void);

//! Run the cron timers if they've fired.
void pbl_cron_wakeup(void);

//! Execute time of the earliest scheduled job, or 0 when none is scheduled.
time_t pbl_cron_get_next_execute_time(void);
#endif
