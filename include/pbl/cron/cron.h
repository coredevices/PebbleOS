/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "pbl/util/list.h"

/**
 * @defgroup cron Cron
 * @ingroup subsys
 * @brief Wall-clock timers, for alarms, calendar events and the like.
 *
 * A job matches a set of local times, like a crontab entry: each of the minute, hour, day of the
 * month and month fields is a value or "any", and the days of the week are a mask. Scheduling a
 * job computes its next matching time; the job fires once, then is unscheduled. DST transitions
 * are handled, and clock changes recompute the execution times as configured per job.
 *
 * Callbacks run on the NewTimers thread, or on the caller of pbl_cron_handle_clock_change() for
 * the jobs it finds due, without the cron lock held, so they may reschedule their own job. The
 * job structures are owned by the caller and must stay valid while scheduled.
 *
 * @code{.c}
 * static void prv_alarm_fired(struct pbl_cron_job *job, void *data) {
 *   ...
 *   pbl_cron_job_schedule(job);  // fire again on the next match
 * }
 *
 * static struct pbl_cron_job s_alarm = {
 *   .cb = prv_alarm_fired,
 *   .minute = 30,
 *   .hour = 7,
 *   .mday = PBL_CRON_MDAY_ANY,
 *   .month = PBL_CRON_MONTH_ANY,
 *   .wday = PBL_CRON_WDAY_WEEKDAYS,
 *   .clock_change_tolerance = 60,
 * };
 *
 * time_t when = pbl_cron_job_schedule(&s_alarm);  // next weekday at 07:30
 * @endcode
 * @{
 */

struct pbl_cron_job;

/**
 * @brief Callback of a cron job.
 *
 * The job is already unscheduled when the callback runs.
 *
 * @param job Job that fired.
 * @param data @ref pbl_cron_job::cb_data.
 */
typedef void (*pbl_cron_job_cb_t)(struct pbl_cron_job *job, void *data);

/** @brief Any minute. */
#define PBL_CRON_MINUTE_ANY (-1)
/** @brief Any hour. */
#define PBL_CRON_HOUR_ANY (-1)
/** @brief Any day of the month. */
#define PBL_CRON_MDAY_ANY (-1)
/** @brief Any month. */
#define PBL_CRON_MONTH_ANY (-1)

/** @brief Sunday in a day of the week mask. */
#define PBL_CRON_WDAY_SUNDAY (1 << 0)
/** @brief Monday in a day of the week mask. */
#define PBL_CRON_WDAY_MONDAY (1 << 1)
/** @brief Tuesday in a day of the week mask. */
#define PBL_CRON_WDAY_TUESDAY (1 << 2)
/** @brief Wednesday in a day of the week mask. */
#define PBL_CRON_WDAY_WEDNESDAY (1 << 3)
/** @brief Thursday in a day of the week mask. */
#define PBL_CRON_WDAY_THURSDAY (1 << 4)
/** @brief Friday in a day of the week mask. */
#define PBL_CRON_WDAY_FRIDAY (1 << 5)
/** @brief Saturday in a day of the week mask. */
#define PBL_CRON_WDAY_SATURDAY (1 << 6)

/** @brief Monday to Friday. */
#define PBL_CRON_WDAY_WEEKDAYS                                              \
  (PBL_CRON_WDAY_MONDAY | PBL_CRON_WDAY_TUESDAY | PBL_CRON_WDAY_WEDNESDAY | \
   PBL_CRON_WDAY_THURSDAY | PBL_CRON_WDAY_FRIDAY)
/** @brief Saturday and Sunday. */
#define PBL_CRON_WDAY_WEEKENDS (PBL_CRON_WDAY_SUNDAY | PBL_CRON_WDAY_SATURDAY)
/** @brief Every day of the week. */
#define PBL_CRON_WDAY_ANY (PBL_CRON_WDAY_WEEKENDS | PBL_CRON_WDAY_WEEKDAYS)

/** @brief Cron job. Fill in the schedule and callback, then call pbl_cron_job_schedule(). */
struct pbl_cron_job {
  /** Node in the list of scheduled jobs; internal. */
  ListNode list_node;

  /**
   * Execution time in seconds since the epoch, set by pbl_cron_job_schedule(). Must not be
   * changed while the job is scheduled.
   */
  time_t cached_execute_time;

  /** Called when the job fires. */
  pbl_cron_job_cb_t cb;
  /** Data passed to @ref cb. */
  void *cb_data;

  /**
   * Clock change, in seconds, from which the execution time is recalculated.
   *
   * A time zone or DST change always recalculates. A change of the time itself (set by the user
   * or by the phone) recalculates when it is at least this large, so 0 always recalculates and
   * UINT32_MAX never does. A recalculated job that was skipped over waits for its next match; a
   * job that is not recalculated and was skipped over fires immediately.
   */
  uint32_t clock_change_tolerance;

  /** Minute, 0 to 59, or @ref PBL_CRON_MINUTE_ANY. */
  int8_t minute;
  /** Hour, 0 to 23, or @ref PBL_CRON_HOUR_ANY. */
  int8_t hour;
  /** Day of the month, 0-based (0 to 30), or @ref PBL_CRON_MDAY_ANY. */
  int8_t mday;
  /** Month, 0 to 11, or @ref PBL_CRON_MONTH_ANY. */
  int8_t month;

  /**
   * Offset in seconds applied to the matched time. A job for Monday 0:15 with an offset of
   * -1800 fires on Sunday at 23:45.
   */
  int32_t offset_seconds;

  /** Scheduled with pbl_cron_job_schedule_at(); internal. */
  bool absolute;

  union {
    /** All flags. */
    uint8_t flags;

    struct {
      /** Days of the week, a PBL_CRON_WDAY_ mask; 0 acts like @ref PBL_CRON_WDAY_ANY. */
      uint8_t wday : 7;

      /**
       * Allow the execution time to be the current time, for events that must happen at the
       * specified time even if that is right now.
       */
      bool may_be_instant : 1;
    };
  };
};

/**
 * @brief Initialize the cron subsystem.
 */
void pbl_cron_init(void);

/**
 * @brief Adjust the scheduled jobs after a wall clock change.
 *
 * Recalculates execution times as described for @ref pbl_cron_job::clock_change_tolerance,
 * then runs the jobs that are due.
 *
 * @param utc_time_delta Seconds the UTC time moved by.
 * @param gmt_offset_delta Seconds the GMT offset moved by; non-zero forces recalculation.
 * @param dst_changed Whether the DST state changed; true forces recalculation.
 */
void pbl_cron_handle_clock_change(int32_t utc_time_delta, int32_t gmt_offset_delta,
                                  bool dst_changed);

/**
 * @brief Schedule a job, or reschedule it if already scheduled.
 *
 * The job runs once, at its next matching time. The subsystem references the job until it fires
 * or is unscheduled.
 *
 * @param job Job.
 * @return Execution time, in seconds since the epoch.
 */
time_t pbl_cron_job_schedule(struct pbl_cron_job *job);

/**
 * @brief Schedule a job at a fixed time, or reschedule it if already scheduled.
 *
 * The job runs once at @p utc_time; its schedule fields are ignored. The time does not follow
 * time zone, DST or clock changes: a job the clock is moved past runs right away.
 *
 * @param job Job.
 * @param utc_time Execution time, in seconds since the epoch.
 */
void pbl_cron_job_schedule_at(struct pbl_cron_job *job, time_t utc_time);

/**
 * @brief Schedule a job to run right after another one.
 *
 * @p new_job takes the schedule of @p job and keeps its own callback and data. It fires after
 * @p job, though not necessarily immediately after.
 *
 * @param job Scheduled job.
 * @param new_job Job to schedule, not scheduled.
 * @return Execution time, in seconds since the epoch.
 */
time_t pbl_cron_job_schedule_after(struct pbl_cron_job *job, struct pbl_cron_job *new_job);

/**
 * @brief Unschedule a job.
 *
 * @param job Job.
 * @return true if the job was removed, false if it was not scheduled (including while its
 * callback runs).
 */
bool pbl_cron_job_unschedule(struct pbl_cron_job *job);

/**
 * @brief Check whether a job is scheduled.
 *
 * @param job Job.
 * @return true if scheduled.
 */
bool pbl_cron_job_is_scheduled(struct pbl_cron_job *job);

/**
 * @brief Compute the next execution time of a job from the current time.
 *
 * @param job Job.
 * @return Execution time, in seconds since the epoch.
 */
time_t pbl_cron_job_get_execute_time(const struct pbl_cron_job *job);

/**
 * @brief Compute the next execution time of a job from a given time.
 *
 * @param job Job.
 * @param local_epoch Time to compute from, in seconds since the epoch.
 * @return Execution time, in seconds since the epoch.
 */
time_t pbl_cron_job_get_execute_time_from_epoch(const struct pbl_cron_job *job, time_t local_epoch);

#if UNITTEST
/** @brief Unschedule every job. Unit tests only. */
void pbl_cron_clear_all_jobs(void);

/** @brief Unschedule every job and stop the wakeup timer. Unit tests only. */
void pbl_cron_deinit(void);

/**
 * @brief Count the scheduled jobs. Unit tests only.
 *
 * @return Number of scheduled jobs.
 */
uint32_t pbl_cron_get_job_count(void);

/** @brief Run the jobs that are due. Unit tests only. */
void pbl_cron_wakeup(void);

/**
 * @brief Get the execution time of the earliest scheduled job. Unit tests only.
 *
 * @return Execution time, or 0 when no job is scheduled.
 */
time_t pbl_cron_get_next_execute_time(void);
#endif

/** @} */
