/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/cron/cron.h>
#include <pbl/i18n/i18n.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/activity/activity.h>
#include <pbl/services/alarms/alarm.h>
#include <pbl/services/alarms/alarm_pin.h>
#include <pbl/services/clock.h>
#include <pbl/services/time.h>
#include <pbl/services/timeline/event.h>
#include <pbl/settings/settings.h>
#include <pbl/util/string.h>
#include <pbl/util/testing.h>
#include <pbl/util/units.h>

#include <applib/event_service_client.h>
#include <apps/system_app_ids.h>
#include <kernel/events.h>
#include <process_management/app_install_manager.h>

PBL_LOG_MODULE_DEFINE(service_alarms, CONFIG_SERVICE_ALARMS_LOG_LEVEL);

#define DEFAULT_SNOOZE_DELAY_M (10)
#define MAX_CONFIGURED_ALARMS  (10)

#define ALARM_FILE_NAME          "/alarms"
#define ALARM_MAX_FILE_SIZE      PBL_KIB(1) // ~50 alarms or so
#define NUM_ALARM_PINS_PER_ALARM (3)
#define ALARM_ENTRY_SIZE         (UUID_SIZE * NUM_ALARM_PINS_PER_ALARM)

// All alarm preferences are saved in the file under separate keys to simplify
// backward compatibility. When a new preference is added, watches with older
// firmwares won't have that preference stored and the preference-loading code
// naturally falls back to setting the preference to its default. If the
// preferences were all stored as a struct under a single key, the structs would
// need to be versioned and newer firmwares would need to know how to parse or
// migrate older versions of the preference struct.
#define ALARM_PREF_KEY_SNOOZE_DELAY "SnoozeDelayM"
// The alarm that is currently armed, so that a firing missed while the watch was
// down (a reboot landing on the alarm's time) can be caught up on the next boot.
#define ALARM_PREF_KEY_ARMED "ArmedAlarm"

// How late a missed alarm may be before it is no longer worth firing.
#define ALARM_MISSED_MAX_DELAY_S (5 * PBL_SEC_PER_MIN)

typedef struct PBL_PACKED AlarmArmedRecord {
  //! Cron execute time of the armed alarm, 0 if no alarm is armed.
  int32_t time;
  AlarmId id;
} AlarmArmedRecord;

// Multiple pieces of data need to be stored for each alarm. These are split
// across multiple keys to keep the alarm configuration separate from
// bookkeeping data.
typedef enum AlarmDataType {
  ALARM_DATA_CONFIG = 0,
  ALARM_DATA_PINS = 1,
} AlarmDataType;

// Stored alarm data is keyed off a binary (AlarmId, AlarmDataType) tuple
// so that programmatic construction of a key is straightforward.
typedef struct PBL_PACKED AlarmStorageKey {
  AlarmId id;
  AlarmDataType type : 8;
} AlarmStorageKey;

typedef struct PBL_PACKED {
  AlarmKind kind : 8;
  //! Whether the alarm is disabled or not. This field cannot be updated to a bitfield because the
  //! compiler sets arbitrary bits to indicate true as an optimization.
  bool is_disabled;
  uint8_t hour;
  uint8_t minute;
  // 1 entry per week day. True if the alarm should go off on that week day. Sunday = 0.
  bool scheduled_days[PBL_DAY_PER_WEEK];
  //! v3.12 alarm fields, compiled in even for unhealthy platforms to simplify compatibility
  union {
    //! These flags have the same value in memory and in flash.
    struct {
      //! Whether the alarm is a smart alarm or not. Smart alarms attempt to wake the user the
      //! first moment the user is not in deep sleep in the time range T-30min to T every 5 min.
      bool is_smart : 1;
      //! Whether the alarm should play a tone on speaker hardware. Default 0 (off) so legacy
      //! alarms loaded from older firmware stay silent.
      bool sound_enabled : 1;
      //! Whether vibration is *disabled* for this alarm. Default 0 (vibration on) so legacy
      //! alarms loaded from older firmware keep vibrating.
      bool vibrate_disabled : 1;
      //! Selected tone (AlarmTone enum value). Default 0 (Reveille). Irrelevant when
      //! sound_enabled is 0.
      uint8_t tone : 3;
    };
    uint8_t flags;
  };
} AlarmConfig;

typedef struct Alarm {
  AlarmId id;
  AlarmConfig config;
} Alarm;

typedef bool (*AlarmOperationCallback)(AlarmId id, AlarmConfig *config, void *context);

// Forward declarations
static void prv_alarm_operation(AlarmId id, AlarmOperationCallback callback, void *context,
                                bool schedule_changed);
static bool prv_reload_alarms(struct pbl_settings_file *file);
static bool prv_alarm_get_config(struct pbl_settings_file *file, AlarmId id,
                                 AlarmConfig *config_out);
static void prv_alarm_set_config(struct pbl_settings_file *file, AlarmId id,
                                 const AlarmConfig *config);
static void prv_cron_callback(struct pbl_cron_job *job, void *data);
static void prv_snooze_alarm(int snooze_delay_s, bool user_initiated);
static void prv_snooze_timer_callback(struct pbl_work *work);
static bool prv_set_alarm_kind_op(AlarmId id, AlarmConfig *config, void *context);
static bool prv_set_alarm_custom_op(AlarmId id, AlarmConfig *config, void *context);

// Private globals
static bool s_alarms_enabled = false;
static time_t s_next_alarm_time;
//! Whether the next alarm uses the smart-alarm early window.
static bool s_next_alarm_smart_window;

//! This is only valid when s_next_alarm_time is not 0.
static Alarm s_next_alarm;
static struct pbl_cron_job s_next_alarm_cron;
static AlarmId s_most_recent_alarm_id = ALARM_INVALID_ID;
static AlarmConfig s_most_recent_alarm_config;
//! Configured occurrence that last fired for each alarm ID. A reload must not re-arm it.
//! RAM-only: a boot reconstructs the next occurrence from the armed record.
static time_t s_fired_occurrences[MAX_CONFIGURED_ALARMS];
//! Generation of the cron callback for each slot. Bumped when the slot is created, deleted,
//! disabled, or rescheduled so a callback queued for an old alarm cannot fire a new one.
static uintptr_t s_alarm_fire_generations[MAX_CONFIGURED_ALARMS];
static bool s_most_recent_alarm_smart_window;
static bool s_most_recent_alarm_recorded;

PBL_T_STATIC PBL_DELAYED_WORK_DEFINE(s_snooze_timer, prv_snooze_timer_callback);
static uintptr_t s_snooze_generation;
static uint16_t s_snooze_delay_m = DEFAULT_SNOOZE_DELAY_M;

//! Whether the pending snooze timer was armed by an explicit user snooze rather
//! than the smart alarm's internal sleep poll.
static bool s_user_snoozed;

//! Number of times the most recent alarm was automatically smart snoozed.
//! Used to determine an expired smart alarm avoiding issues with midnight rollover and DST.
static int s_smart_snooze_counter;

//! Mirrors what ALARM_PREF_KEY_ARMED holds, so it is only rewritten when it changes.
static AlarmArmedRecord s_armed_record = {.id = ALARM_INVALID_ID};

//! Alarm which should have fired while the watch was down. Fired once alarms are enabled.
static AlarmId s_missed_alarm_id = ALARM_INVALID_ID;
static time_t s_missed_alarm_time;

//! The missed alarm handed to the queued catch-up callback.
static AlarmId s_missed_alarm_catchup_id = ALARM_INVALID_ID;
static time_t s_missed_alarm_catchup_time;
//! Invalidates a queued catch-up when its slot is updated, disabled, deleted, or reused.
static uintptr_t s_missed_alarm_generation;

//! Mutex used to guard our list of alarms
static PBL_MUTEX_DEFINE(s_mutex);

// ----------------------------------------------------------------------------------------------
static bool prv_file_open_and_lock(struct pbl_settings_file *file) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  bool success = pbl_settings_open(file, ALARM_FILE_NAME, ALARM_MAX_FILE_SIZE) == 0;
  if (!success) {
    pbl_mutex_unlock(&s_mutex);
  }

  return success;
}

// ----------------------------------------------------------------------------------------------
static void prv_file_close_and_unlock(struct pbl_settings_file *file) {
  pbl_settings_close(file);
  pbl_mutex_unlock(&s_mutex);
}

// ----------------------------------------------------------------------------------------------
static ActivitySleepState prv_get_sleep_state(void) {
  int32_t sleep_state;
  const bool rv = activity_get_metric(ActivityMetricSleepState, 1, &sleep_state);
  return rv ? sleep_state : ActivitySleepStateUnknown;
}

// ----------------------------------------------------------------------------------------------
static int32_t prv_get_vmc(void) {
  int32_t vmc;
  const bool rv = activity_get_metric(ActivityMetricLastVMC, 1, &vmc);
  return rv ? vmc : 0;
}

// ----------------------------------------------------------------------------------------------
static bool prv_should_smart_alarm_trigger(const AlarmConfig *config) {
  if (s_smart_snooze_counter >= SMART_ALARM_MAX_SMART_SNOOZE) {
    // The smart alarm has reached the end of its time range
    return true;
  }
  switch (prv_get_sleep_state()) {
    case ActivitySleepStateUnknown:
    case ActivitySleepStateAwake:
      // The user is awake, just trigger
      return true;
    case ActivitySleepStateLightSleep:
    case ActivitySleepStateRestfulSleep:
      // The user is asleep, trigger if VMC > 0
      return prv_get_vmc() > 0;
  }
  return false;
}

// ----------------------------------------------------------------------------------------------
//! Removes the alarm from the timeline. No-op if it doesn't exist.
//! @return true if an alarm pin was removed
static bool prv_timeline_remove_alarm(struct pbl_settings_file *fd, AlarmId id) {
  bool success = false;
  AlarmStorageKey key = {.id = id, .type = ALARM_DATA_PINS};
  int size = pbl_settings_get_len(fd, &key, sizeof(key));

  if (size <= 0) { // empty (likely deleted) entry
    return false;
  } else if (size > ALARM_ENTRY_SIZE) { // malformed data
    pbl_settings_delete(fd, &key, sizeof(key));
    return false;
  } else { // size <= ALARM_ENTRY_SIZE
    uint8_t buffer[size];
    if (pbl_settings_get(fd, &key, sizeof(key), buffer, size) == 0) {
      for (int i = 0; i < size / UUID_SIZE; i++) {
        Uuid *pinid = (Uuid *)&buffer[UUID_SIZE * i];
        if (uuid_is_invalid(pinid)) {
          continue;
        }
        alarm_pin_remove(pinid);
        success = true;
      }
      pbl_settings_delete(fd, &key, sizeof(key));
    }
  }
  return success;
}

// ----------------------------------------------------------------------------------------------
//! Converts an alarm's cron time to its actual alarm time.
//! Smart alarms execute earlier than their actual alarm time in order to opportunistically alarm.
static time_t prv_get_alarm_time(time_t cron_time, bool smart_window) {
  return smart_window ? cron_time + SMART_ALARM_RANGE_S : cron_time;
}

// ----------------------------------------------------------------------------------------------
//! Pin add helper
static int prv_add_pin(AlarmId id, const AlarmConfig *config, time_t alarm_time, Uuid *uuid_out) {
  const AlarmType type = config->is_smart ? AlarmType_Smart : AlarmType_Basic;
  return alarm_pin_add(alarm_time, id, type, config->kind, uuid_out);
}

// ----------------------------------------------------------------------------------------------
//! Pins alarm in the timeline for the next three days
static void prv_timeline_add_alarm(struct pbl_settings_file *file, const Alarm *alarm,
                                   const struct pbl_cron_job *cron, const time_t current_time,
                                   bool smart_window) {
  // If an alarm was updated then remove all the pins with stale information
  // If an alarm was added then this has no effect
  prv_timeline_remove_alarm(file, alarm->id);

  // We allocate some larger variables on the heap to reduce stack usage
  struct pbl_tm *local_alarm_time = pbl_malloc_check(sizeof(struct pbl_tm));
  uint8_t *settings_file_buffer = pbl_malloc_check(ALARM_ENTRY_SIZE);
  AppInstallEntry *entry = pbl_malloc_check(sizeof(AppInstallEntry));
  if (!app_install_get_entry_for_install_id(APP_ID_ALARMS, entry)) {
    goto cleanup;
  }

  int num_pin_adds = 0;
  time_t alarm_time = prv_get_alarm_time(pbl_cron_job_get_execute_time(cron), smart_window);

  time_t last_alarm = 0;
  for (int i = 0; alarm_time <= current_time + PBL_SEC_PER_DAY * 3; i++) {
    if (last_alarm != alarm_time) {
      last_alarm = alarm_time;
      pbl_localtime_r(&alarm_time, local_alarm_time);
      if (num_pin_adds < NUM_ALARM_PINS_PER_ALARM &&
          alarm->config.scheduled_days[local_alarm_time->tm_wday]) {
        Uuid *pinid = (Uuid *)&settings_file_buffer[num_pin_adds * UUID_SIZE];
        int rv = prv_add_pin(alarm->id, &alarm->config, alarm_time, pinid);
        if (rv == 0) {
          num_pin_adds++;
        } else {
          PBL_LOG_ERR("Could not add alarm %u pin: %d", alarm->id, rv);
        }
      }
    }
    alarm_time = prv_get_alarm_time(
        pbl_cron_job_get_execute_time_from_epoch(cron, current_time + (i * PBL_SEC_PER_DAY)),
        smart_window);
  }

  AlarmStorageKey key = {.id = alarm->id, .type = ALARM_DATA_PINS};
  int rv =
      pbl_settings_set(file, &key, sizeof(key), settings_file_buffer, UUID_SIZE * num_pin_adds);
  if (rv < 0) {
    PBL_LOG_ERR("Could not save alarm %u pin IDs: %d", alarm->id, rv);
    for (int i = 0; i < num_pin_adds; ++i) {
      alarm_pin_remove((Uuid *)&settings_file_buffer[i * UUID_SIZE]);
    }
  }

cleanup:
  pbl_free(settings_file_buffer);
  pbl_free(local_alarm_time);
  pbl_free(entry);
}

// ----------------------------------------------------------------------------------------------
static void prv_build_cron(const AlarmConfig *config, struct pbl_cron_job *cron,
                           bool *smart_window_out) {
  const bool smart_window = config->is_smart && activity_tracking_on();
  if (smart_window_out) {
    *smart_window_out = smart_window;
  }
  *cron = (struct pbl_cron_job){
    .cb = prv_cron_callback,
    .cb_data = (void *)0,

    .minute = config->minute,
    .hour = config->hour,
    .mday = PBL_CRON_MDAY_ANY,
    .month = PBL_CRON_MONTH_ANY,

    .offset_seconds = smart_window ? -SMART_ALARM_RANGE_S : 0,

    // Tolerate up to a 15 minute clock change before recalculating.
    .clock_change_tolerance = 15 * PBL_SEC_PER_MIN,
  };
  for (int i = 0; i < PBL_DAY_PER_WEEK; i++) {
    cron->wday |= config->scheduled_days[i] ? (1 << i) : 0;
  }
}

// ----------------------------------------------------------------------------------------------
typedef struct AlarmOccurrence {
  struct pbl_cron_job cron;
  time_t execute_time;
  bool smart_window;
} AlarmOccurrence;

//! Whether this alarm's configured occurrence is exactly now.
static bool prv_alarm_is_due_now(const AlarmConfig *config, time_t now) {
  struct pbl_tm tm;
  pbl_localtime_r(&now, &tm);
  return tm.tm_sec == 0 && tm.tm_hour == config->hour && tm.tm_min == config->minute &&
         config->scheduled_days[tm.tm_wday];
}

//! Whether this exact occurrence has already fired.
static bool prv_occurrence_already_fired(AlarmId id, time_t occurrence_time) {
  return id >= 0 && id < MAX_CONFIGURED_ALARMS && occurrence_time != 0 &&
         s_fired_occurrences[id] == occurrence_time;
}

//! Forget the occurrence fired by @p id, e.g. when its slot is deleted or reused.
static void prv_clear_fired_occurrence(AlarmId id) {
  if (id >= 0 && id < MAX_CONFIGURED_ALARMS) {
    s_fired_occurrences[id] = 0;
  }
}

//! Pack an alarm ID and fire generation into the cron callback's single data pointer.
#define ALARM_FIRE_ID_BITS (8)
#define ALARM_FIRE_ID_MASK ((uintptr_t)0xff)

static void *prv_alarm_fire_token(AlarmId id, uintptr_t generation) {
  return (void *)((generation << ALARM_FIRE_ID_BITS) | ((uintptr_t)id & ALARM_FIRE_ID_MASK));
}

static AlarmId prv_alarm_fire_id(const void *token) {
  return (AlarmId)((uintptr_t)token & ALARM_FIRE_ID_MASK);
}

static uintptr_t prv_alarm_fire_generation(const void *token) {
  return (uintptr_t)token >> ALARM_FIRE_ID_BITS;
}

//! Invalidate a cron callback already queued for @p id.
static void prv_invalidate_alarm_fire(AlarmId id) {
  if (id >= 0 && id < MAX_CONFIGURED_ALARMS) {
    s_alarm_fire_generations[id]++;
  }
}

//! Forget any missed-alarm state for @p id, e.g. when its slot is deleted or reused.
//!
//! Must be called with s_mutex held.
static void prv_clear_missed_alarm(AlarmId id) {
  pbl_mutex_assert_held(&s_mutex, true);
  if (s_missed_alarm_id == id) {
    s_missed_alarm_id = ALARM_INVALID_ID;
    s_missed_alarm_time = 0;
  }
  if (s_missed_alarm_catchup_id == id) {
    s_missed_alarm_catchup_id = ALARM_INVALID_ID;
    s_missed_alarm_catchup_time = 0;
    s_missed_alarm_generation++;
  }
}

//! Build the cron job for the alarm's configured time, ignoring the smart window.
static void prv_build_base_cron(const AlarmConfig *config, struct pbl_cron_job *cron) {
  AlarmConfig base_config = *config;
  base_config.is_smart = false;
  prv_build_cron(&base_config, cron, NULL);
}

//! The configured occurrence that was scheduled to fire at @p execute_time.
//!
//! A smart alarm can fire at its early window, while a basic or tracking-off alarm fires at its
//! configured time. Search just before the execute time so both map to the same occurrence.
static time_t prv_configured_occurrence(const AlarmConfig *config, time_t execute_time) {
  struct pbl_cron_job base_cron;
  prv_build_base_cron(config, &base_cron);
  return pbl_cron_job_get_execute_time_from_epoch(&base_cron, execute_time - 1);
}

//! Select the next occurrence and whether it uses the smart-alarm window.
//!
//! If tracking starts after the window has begun, keep the configured occurrence instead of
//! letting cron's negative offset push it to the next day.
static void prv_select_alarm_occurrence(AlarmId id, const AlarmConfig *config, time_t now,
                                        AlarmOccurrence *occurrence) {
  struct pbl_cron_job base_cron;
  prv_build_base_cron(config, &base_cron);
  const time_t configured_time = pbl_cron_job_get_execute_time_from_epoch(&base_cron, now);

  // A non-instant cron job advances past an occurrence at exactly its time. If this occurrence
  // has not already fired, keep it so a reload (e.g. tracking starting) cannot skip it.
  // A reload between the cron fire and the callback recording the occurrence can still re-arm
  // it here; that window is sub-second and the callback records it shortly after.
  if (prv_alarm_is_due_now(config, now) && !prv_occurrence_already_fired(id, now)) {
    occurrence->cron = base_cron;
    occurrence->cron.may_be_instant = true;
    occurrence->execute_time = now;
    occurrence->smart_window = false;
    return;
  }

  const bool smart_window = config->is_smart && activity_tracking_on();
  const bool already_fired = prv_occurrence_already_fired(id, configured_time);
  if (smart_window && !already_fired && configured_time >= now &&
      configured_time - SMART_ALARM_RANGE_S <= now) {
    occurrence->cron = base_cron;
    occurrence->execute_time = configured_time;
    occurrence->smart_window = false;
    return;
  }

  prv_build_cron(config, &occurrence->cron, &occurrence->smart_window);
  // When this occurrence already fired, advance past its configured time. Using now + 1 is not
  // enough if tracking turned off: cron then has no offset and would select the fired time again.
  occurrence->execute_time = pbl_cron_job_get_execute_time_from_epoch(
      &occurrence->cron, already_fired ? configured_time + 1 : now);
}

// ----------------------------------------------------------------------------------------------
static void prv_assign_alarm(Alarm *alarm, struct pbl_cron_job *cron, time_t execute_time,
                             bool smart_window) {
  pbl_cron_job_unschedule(&s_next_alarm_cron);
  s_next_alarm_cron = *cron;
  s_next_alarm_cron.cb_data = prv_alarm_fire_token(alarm->id, s_alarm_fire_generations[alarm->id]);
  s_next_alarm = *alarm;
  s_next_alarm_smart_window = smart_window;
  // This marks the job absolute, opting out of cron's clock-change recalculation.
  // alarm_handle_clock_change() reloads and re-arms on the same significant changes.
  pbl_cron_job_schedule_at(&s_next_alarm_cron, execute_time);
  s_next_alarm_time = execute_time;
  PBL_LOG_INFO("Scheduling alarm %u to go off at %d:%d (%ld) (smart:%d)", alarm->id,
               alarm->config.hour, alarm->config.minute, (long)s_next_alarm_time,
               alarm->config.is_smart);
}

// ----------------------------------------------------------------------------------------------
//! Checks to see if this alarm is the new next alarm. If it is, adjust the timer so it's scheduled.
static void prv_check_and_schedule_alarm(struct pbl_settings_file *fd, Alarm *alarm, bool refresh) {
  if (alarm->id == ALARM_INVALID_ID) {
    return;
  }

  if (alarm->config.is_disabled) {
    prv_timeline_remove_alarm(fd, alarm->id);
    return;
  }

  const time_t now = pbl_realtime();
  AlarmOccurrence occurrence;
  prv_select_alarm_occurrence(alarm->id, &alarm->config, now, &occurrence);

  prv_timeline_add_alarm(fd, alarm, &occurrence.cron, now, occurrence.smart_window);

  if (s_next_alarm_time == 0 || occurrence.execute_time < s_next_alarm_time) {
    // This alarm is sooner than the previous one!
    prv_assign_alarm(alarm, &occurrence.cron, occurrence.execute_time, occurrence.smart_window);
  }
  if (refresh) {
    timeline_event_refresh();
  }
}

// ----------------------------------------------------------------------------------------------
//! Records which alarm is armed and when it is due, so that the next boot can tell whether a
//! firing was missed while the watch was down.
static void prv_persist_armed_alarm(struct pbl_settings_file *file) {
  const AlarmArmedRecord record = {
    .time = s_next_alarm_time,
    .id = (s_next_alarm_time != 0) ? s_next_alarm.id : ALARM_INVALID_ID,
  };
  if (memcmp(&record, &s_armed_record, sizeof(record)) == 0) {
    return;
  }
  if (pbl_settings_set(file, ALARM_PREF_KEY_ARMED, strlen(ALARM_PREF_KEY_ARMED), &record,
                       sizeof(record)) == 0) {
    s_armed_record = record;
  }
}

// ----------------------------------------------------------------------------------------------
static int prv_remove_untracked_alarm_pins(struct pbl_settings_file *file) {
  Uuid *tracked = pbl_malloc_check(MAX_CONFIGURED_ALARMS * ALARM_ENTRY_SIZE);
  size_t tracked_count = 0;
  int rv = 0;

  for (AlarmId id = 0; id < MAX_CONFIGURED_ALARMS; ++id) {
    AlarmStorageKey key = {.id = id, .type = ALARM_DATA_PINS};
    const size_t size = pbl_settings_get_len(file, &key, sizeof(key));
    if (size == 0 || size > ALARM_ENTRY_SIZE || size % UUID_SIZE != 0) {
      continue;
    }
    rv = pbl_settings_get(file, &key, sizeof(key), &tracked[tracked_count], size);
    if (rv < 0) {
      break;
    }
    tracked_count += size / UUID_SIZE;
  }

  if (rv == 0) {
    rv = alarm_pin_remove_untracked_future(pbl_realtime(), tracked, tracked_count);
  }
  pbl_free(tracked);
  return rv;
}

// ----------------------------------------------------------------------------------------------
//! Scans the all the configured alarms and re-adds them all.
//! @return True if at least one alarm was found
static bool prv_reload_alarms(struct pbl_settings_file *file) {
  bool alarm_found = false;

  int rv = prv_remove_untracked_alarm_pins(file);
  if (rv < 0) {
    PBL_LOG_ERR("Could not remove untracked alarm pins: %d", rv);
  }

  s_next_alarm_time = 0;
  s_next_alarm_smart_window = false;
  pbl_cron_job_unschedule(&s_next_alarm_cron);

  for (int i = 0; i < MAX_CONFIGURED_ALARMS; ++i) {
    AlarmConfig config;
    if (prv_alarm_get_config(file, i, &config)) {
      Alarm alarm = {.id = i, .config = config};
      prv_check_and_schedule_alarm(file, &alarm, false /* refresh */);
      alarm_found = true;
    }
  }

  prv_persist_armed_alarm(file);

  timeline_event_refresh();
  return alarm_found;
}

// ----------------------------------------------------------------------------------------------
static void prv_put_alarm_event(void) {
  if (!s_alarms_enabled) {
    return;
  }

  AlarmConfig *config =
      s_most_recent_alarm_id != ALARM_INVALID_ID ? &s_most_recent_alarm_config : nullptr;
  const bool is_smart = config && s_most_recent_alarm_smart_window;
  PebbleEvent e = (PebbleEvent){
    .type = PEBBLE_ALARM_CLOCK_EVENT,
    .alarm_clock = {
      .alarm_time = pbl_realtime(),
      .alarm_label = is_smart ? pbl_i18n_noop("Smart Alarm") : pbl_i18n_noop("Alarm")
    }
  };

  event_put(&e);
}

// ----------------------------------------------------------------------------------------------
static bool prv_record_alarm_op(AlarmId id, AlarmConfig *config, void *context) {
  // Add a pin to the timeline (will show up in the past)
  prv_add_pin(id, config, pbl_realtime(), nullptr);

  return false;
}

// ----------------------------------------------------------------------------------------------
static void prv_clear_snooze_timer(void) {
  pbl_delayed_work_cancel(&s_snooze_timer);
  // Invalidate callbacks already queued on SysWorkq.
  s_snooze_generation++;
  s_user_snoozed = false;
}

// ----------------------------------------------------------------------------------------------
// Put a trigger event if applicable based on the type of alarm.
//!
//! @param require_alarm Whether a valid most-recent alarm is required. Alarm-fire timer callbacks
//!        pass true so a slot deleted after the callback was queued cannot produce an anonymous
//!        popup. The snooze callback passes false so an anonymous user snooze still fires.
static void prv_process_most_recent_alarm(bool require_alarm) {
  if (require_alarm && s_most_recent_alarm_id == ALARM_INVALID_ID) {
    return;
  }

  // Only processes the most recent alarm since it modifies the alarm config
  AlarmConfig *config =
      s_most_recent_alarm_id != ALARM_INVALID_ID ? &s_most_recent_alarm_config : nullptr;
  const bool user_snoozed = s_user_snoozed;
  s_user_snoozed = false;
  bool trigger = true;
  const bool is_smart = config && s_most_recent_alarm_smart_window;
  // A user snooze must fire after exactly the configured delay; only the smart
  // alarm's internal sleep poll may re-evaluate the sleep state and re-snooze.
  if (is_smart && !user_snoozed) {
    trigger = prv_should_smart_alarm_trigger(config);
    if (!trigger) {
      // Not triggering an event, increment to signify elapsed time and snooze
      s_smart_snooze_counter++;
      prv_snooze_alarm(SMART_ALARM_SNOOZE_DELAY_S, false /* user_initiated */);
      return;
    }
  }
  if (trigger) {
    // The smart window is over; a stale counter would let a later clock change re-fire it.
    s_smart_snooze_counter = 0;
    prv_put_alarm_event();
    // Alarm events are triggered for both the initial alarm time and subsequent user snoozes
    // Ensure that we only record the first alarm time
    if (!s_most_recent_alarm_recorded) {
      s_most_recent_alarm_recorded = true;
      // Read from flash since the in-memory cache can be modified
      prv_alarm_operation(s_most_recent_alarm_id, prv_record_alarm_op, nullptr, false);
    }
  }
}

// ----------------------------------------------------------------------------------------------
static void prv_snooze_kernel_bg_callback(void *generation) {
  if ((uintptr_t)generation == s_snooze_generation) {
    prv_process_most_recent_alarm(false /* require_alarm */);
  }
}

// ----------------------------------------------------------------------------------------------
static void prv_snooze_timer_callback(struct pbl_work *work) {
  // Read before anything blocks, or a snooze cleared meanwhile would pass as current.
  const uintptr_t generation = s_snooze_generation;

  PBL_LOG_INFO("Snooze timeout");
  pbl_sys_work_call(prv_snooze_kernel_bg_callback, (void *)generation);
}

// ----------------------------------------------------------------------------------------------
//! Fire the alarm at @p id unless its queued callback is stale or it was deleted or disabled.
//!
//! Scheduled callbacks pass the generation captured when they were queued. A missed-alarm
//! catch-up passes its own generation and @ref ALARM_INVALID_ID.
static void prv_fire_alarm(AlarmId id, uintptr_t generation, bool catching_up) {
  if (!catching_up && (id < 0 || id >= MAX_CONFIGURED_ALARMS)) {
    return;
  }

  // We allocate some larger variables on the heap to reduce stack usage
  AlarmConfig *config = &s_most_recent_alarm_config;
  struct pbl_settings_file *file = pbl_malloc_check(sizeof(struct pbl_settings_file));
  if (!prv_file_open_and_lock(file)) {
    pbl_free(file);
    return;
  }

  bool rv = false;
  time_t catchup_time = 0;
  if (catching_up) {
    if (generation != s_missed_alarm_generation || s_missed_alarm_catchup_id < 0 ||
        s_missed_alarm_catchup_id >= MAX_CONFIGURED_ALARMS) {
      goto cleanup;
    }
    id = s_missed_alarm_catchup_id;
    catchup_time = s_missed_alarm_catchup_time;
    // Consume the catch-up even if the alarm vanished or is disabled; do not leave it queued.
    s_missed_alarm_catchup_id = ALARM_INVALID_ID;
    s_missed_alarm_catchup_time = 0;
    s_missed_alarm_generation++;
  } else if (generation != s_alarm_fire_generations[id]) {
    goto cleanup;
  }

  rv = prv_alarm_get_config(file, id, config);
  if (rv && config->is_disabled) {
    rv = false;
  }
  if (!rv) {
    goto cleanup;
  }

  const bool smart_window = (s_next_alarm.id == id) ? s_next_alarm_smart_window
                                                    : (config->is_smart && activity_tracking_on());
  const time_t now = pbl_realtime();
  time_t occurrence_time = 0;
  if (catching_up) {
    // The missed alarm was armed for this occurrence; suppress its configured time even though
    // the reload below schedules the next one. A "just once" alarm may already have been moved to
    // its next day, and it is disabled below, so its original execute time is enough.
    occurrence_time = config->kind == ALARM_KIND_JUST_ONCE
                          ? catchup_time
                          : prv_configured_occurrence(config, catchup_time);
  } else if (s_next_alarm.id == id && s_next_alarm_time != 0 && s_next_alarm_time <= now) {
    occurrence_time = s_next_alarm_time + (s_next_alarm_smart_window ? SMART_ALARM_RANGE_S : 0);
  }
  s_most_recent_alarm_id = id;
  s_most_recent_alarm_smart_window = smart_window;
  if (occurrence_time != 0) {
    s_fired_occurrences[id] = occurrence_time;
  }

  s_smart_snooze_counter = 0;

  // If this is a just once alarm, then disable it.
  if (config->kind == ALARM_KIND_JUST_ONCE) {
    config->is_disabled = true;
    prv_alarm_set_config(file, id, config); // This will reload the alarms
  } else {
    prv_reload_alarms(file);
  }

cleanup:
  prv_file_close_and_unlock(file);
  pbl_free(file);

  if (!rv) {
    return;
  }

  PBL_LOG_INFO("Alarm %u timeout", id);
  s_most_recent_alarm_recorded = false;
  prv_clear_snooze_timer();
  prv_process_most_recent_alarm(true /* require_alarm */);
}

// ----------------------------------------------------------------------------------------------
PBL_T_STATIC void prv_timer_kernel_bg_callback(void *data) {
  prv_fire_alarm(prv_alarm_fire_id(data), prv_alarm_fire_generation(data), false);
}

// ----------------------------------------------------------------------------------------------
PBL_T_STATIC void prv_missed_alarm_catchup_callback(void *data) {
  prv_fire_alarm(ALARM_INVALID_ID, (uintptr_t)data, true);
}

static void prv_cron_callback(struct pbl_cron_job *job, void *data) {
  pbl_sys_work_call(prv_timer_kernel_bg_callback, data);
}

// ----------------------------------------------------------------------------------------------
static void prv_persist_alarm(struct pbl_settings_file *fd, Alarm *alarm) {
  PBL_ASSERT(alarm->id >= 0 && alarm->id < MAX_CONFIGURED_ALARMS, "Invalid id %d", alarm->id);

  AlarmStorageKey key = {.id = alarm->id, .type = ALARM_DATA_CONFIG};
  AlarmConfig config = {
    .kind = alarm->config.kind,
    .is_disabled = alarm->config.is_disabled,
    .hour = alarm->config.hour,
    .minute = alarm->config.minute,
    .is_smart = alarm->config.is_smart,
    .sound_enabled = alarm->config.sound_enabled,
    .vibrate_disabled = alarm->config.vibrate_disabled,
    .tone = alarm->config.tone,
  };
  memcpy(&config.scheduled_days, alarm->config.scheduled_days, sizeof(config.scheduled_days));
  pbl_settings_set(fd, &key, sizeof(key), &config, sizeof(config));
}

// ----------------------------------------------------------------------------------------------
static void prv_add_and_schedule_alarm(struct pbl_settings_file *file, Alarm *alarm) {
  prv_check_and_schedule_alarm(file, alarm, true /* refresh */);
  prv_persist_alarm(file, alarm);
  prv_persist_armed_alarm(file);
}

// ----------------------------------------------------------------------------------------------
static bool prv_alarm_get_config(struct pbl_settings_file *file, AlarmId id,
                                 AlarmConfig *config_out) {
  AlarmStorageKey key = {.id = id, .type = ALARM_DATA_CONFIG};
  const int size = pbl_settings_get_len(file, &key, sizeof(key));
  if (size <= 0) {
    return false;
  }

  AlarmConfig config = {};
  const int load_size = PBL_MIN(size, (int)sizeof(config));
  if (pbl_settings_get(file, &key, sizeof(key), &config, load_size) == 0) {
    if (config.hour > 23 || config.minute > 59) {
      PBL_LOG_DBG(
          "Invalid config for id %u! Blowing it out! "
          "Hours %u Minutes %u Kind %u",
          id, config.hour, config.minute, config.kind);
      pbl_settings_delete(file, &key, sizeof(key));
      return false;
    }

    *config_out = (AlarmConfig){
      .hour = config.hour,
      .minute = config.minute,
      .is_disabled = config.is_disabled,
      .kind = config.kind,
      .is_smart = config.is_smart,
      .sound_enabled = config.sound_enabled,
      .vibrate_disabled = config.vibrate_disabled,
      .tone = config.tone,
    };
    memcpy(&config_out->scheduled_days, config.scheduled_days, sizeof(config.scheduled_days));
    return true;
  }

  return false;
}

// ----------------------------------------------------------------------------------------------
static void prv_alarm_set_config(struct pbl_settings_file *file, AlarmId id,
                                 const AlarmConfig *config) {
  PBL_ASSERTN(id >= 0 && id < MAX_CONFIGURED_ALARMS);

  Alarm alarm = {.id = id, .config = *config};
  prv_persist_alarm(file, &alarm);

  prv_reload_alarms(file);
}

// ----------------------------------------------------------------------------------------------
static AlarmId prv_get_next_free_alarm_id(struct pbl_settings_file *file) {
  AlarmId id_out = ALARM_INVALID_ID;

  for (int i = 0; i < MAX_CONFIGURED_ALARMS; ++i) {
    AlarmStorageKey key = {.id = i, .type = ALARM_DATA_CONFIG};
    if (pbl_settings_get_len(file, &key, sizeof(key)) == 0) {
      id_out = i;
      break;
    }
  }

  return id_out;
}

// ----------------------------------------------------------------------------------------------
static int prv_get_day_for_just_once_alarm(int hour, int minute) {
  // Figure out what day the alarm should happen
  struct pbl_tm local_time;
  const time_t current_time = pbl_realtime();
  pbl_localtime_r(&current_time, &local_time);

  if (hour < local_time.tm_hour || (hour == local_time.tm_hour && minute <= local_time.tm_min)) {
    // The time is before or equal to the current time. Schedule the alarm for tomorrow
    return (local_time.tm_wday + 1) % PBL_DAY_PER_WEEK;
  } else {
    // The time hasn't happened yet today. Schedule it for today
    return local_time.tm_wday;
  }
}

static void prv_set_day_for_just_once_alarm(AlarmConfig *config, int hour, int minute) {
  const bool scheduled_days[PBL_DAY_PER_WEEK] = {false, false, false, false, false, false, false};
  memcpy(&config->scheduled_days, scheduled_days, sizeof(scheduled_days));

  int wday = prv_get_day_for_just_once_alarm(hour, minute);
  config->scheduled_days[wday] = true;
}

// ----------------------------------------------------------------------------------------------
//! Re-picks the day every enabled "just once" alarm should fire on. Their day is stored as a
//! weekday, so an alarm whose firing was missed (the watch was off or rebooting) would otherwise
//! stay armed for the same weekday a whole week later.
static void prv_refresh_just_once_alarm_days(struct pbl_settings_file *file) {
  for (int i = 0; i < MAX_CONFIGURED_ALARMS; ++i) {
    AlarmConfig config;
    if (!prv_alarm_get_config(file, i, &config) || config.kind != ALARM_KIND_JUST_ONCE ||
        config.is_disabled) {
      continue;
    }

    bool previous_days[PBL_DAY_PER_WEEK];
    memcpy(previous_days, config.scheduled_days, sizeof(previous_days));
    prv_set_day_for_just_once_alarm(&config, config.hour, config.minute);
    if (memcmp(previous_days, config.scheduled_days, sizeof(previous_days)) == 0) {
      continue;
    }

    Alarm alarm = {.id = i, .config = config};
    prv_persist_alarm(file, &alarm);
  }
}

// ----------------------------------------------------------------------------------------------
static void prv_assert_alarm_params(int hour, int minute) {
  PBL_ASSERT(hour < 24 && hour >= 0, "Invalid hour value, %d", hour);
  PBL_ASSERT(minute < 60 && minute >= 0, "Invalid minute value, %d", minute);
}

// ----------------------------------------------------------------------------------------------
static void prv_enable_alarm_config(AlarmConfig *config, bool enable) {
  config->is_disabled = !enable;
  if (enable && config->kind == ALARM_KIND_JUST_ONCE) {
    // Update the day required for the alarm
    prv_set_day_for_just_once_alarm(config, config->hour, config->minute);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//! Public Functions

AlarmId alarm_create(const AlarmInfo *info) {
  prv_assert_alarm_params(info->hour, info->minute);
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return ALARM_INVALID_ID;
  }

  AlarmId id = prv_get_next_free_alarm_id(&file);
  prv_invalidate_alarm_fire(id);
  prv_clear_fired_occurrence(id);

  AlarmConfig config = {
    .hour = info->hour,
    .minute = info->minute,
    .kind = info->kind,
    .is_disabled = false,
    .is_smart = info->is_smart,
    .sound_enabled = info->sound_enabled,
    .vibrate_disabled = !info->vibrate_enabled,
    .tone = info->tone,
  };
  if (info->kind == ALARM_KIND_CUSTOM && info->scheduled_days) {
    prv_set_alarm_custom_op(id, &config, (void *)info->scheduled_days);
  } else {
    prv_set_alarm_kind_op(id, &config, (void *)(uintptr_t)info->kind);
  }

  Alarm alarm = {.id = id, .config = config};
  prv_add_and_schedule_alarm(&file, &alarm);
  prv_clear_missed_alarm(id);
  prv_file_close_and_unlock(&file);

  return id;
}

// ----------------------------------------------------------------------------------------------
static void prv_handle_alarm_edit(AlarmId id, bool schedule_changed) {
  if (!schedule_changed) {
    return;
  }

  prv_clear_missed_alarm(id);
  prv_invalidate_alarm_fire(id);

  if (id == s_most_recent_alarm_id) {
    PBL_LOG_DBG("Canceling snooze timer because alarm was edited");
    prv_clear_snooze_timer();
    s_most_recent_alarm_id = ALARM_INVALID_ID;
    s_smart_snooze_counter = 0;
  }

  // The edit re-arms the alarm: allow its configured occurrence to fire again.
  prv_clear_fired_occurrence(id);
}

// ----------------------------------------------------------------------------------------------
typedef bool (*AlarmOperationCallback)(AlarmId id, AlarmConfig *config, void *context);

static void prv_alarm_operation(AlarmId id, AlarmOperationCallback callback, void *context,
                                bool schedule_changed) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  if (!callback(id, &config, context)) {
    goto cleanup;
  }

  prv_handle_alarm_edit(id, schedule_changed);
  prv_enable_alarm_config(&config, true /* enabled */);
  prv_alarm_set_config(&file, id, &config);

cleanup:
  prv_file_close_and_unlock(&file);
}

// ----------------------------------------------------------------------------------------------
typedef struct SetAlarmTimeContext {
  int hour;
  int minute;
} SetAlarmTimeContext;

static bool prv_set_alarm_time_op(AlarmId id, AlarmConfig *config, void *context) {
  SetAlarmTimeContext *ctx = context;
  if (config->kind == ALARM_KIND_JUST_ONCE) {
    // We have to re-calculate the correct day
    prv_set_day_for_just_once_alarm(config, ctx->hour, ctx->minute);
  }
  config->hour = ctx->hour;
  config->minute = ctx->minute;
  return true;
}

void alarm_set_time(AlarmId id, int hour, int minute) {
  prv_assert_alarm_params(hour, minute);
  prv_alarm_operation(id, prv_set_alarm_time_op, &(SetAlarmTimeContext){hour, minute}, true);
}

// ----------------------------------------------------------------------------------------------
static bool prv_set_alarm_smart_op(AlarmId id, AlarmConfig *config, void *context) {
  config->is_smart = (uintptr_t)context;
  return true;
}

void alarm_set_smart(AlarmId id, bool smart) {
  prv_alarm_operation(id, prv_set_alarm_smart_op, (void *)(uintptr_t)smart, true);
}

// ----------------------------------------------------------------------------------------------
static bool prv_set_sound_enabled_op(AlarmId id, AlarmConfig *config, void *context) {
  config->sound_enabled = (uintptr_t)context;
  return true;
}

void alarm_set_sound_enabled(AlarmId id, bool enabled) {
  prv_alarm_operation(id, prv_set_sound_enabled_op, (void *)(uintptr_t)enabled, false);
}

// ----------------------------------------------------------------------------------------------
static bool prv_set_vibrate_enabled_op(AlarmId id, AlarmConfig *config, void *context) {
  config->vibrate_disabled = !(uintptr_t)context;
  return true;
}

void alarm_set_vibrate_enabled(AlarmId id, bool enabled) {
  prv_alarm_operation(id, prv_set_vibrate_enabled_op, (void *)(uintptr_t)enabled, false);
}

// ----------------------------------------------------------------------------------------------
static bool prv_set_tone_op(AlarmId id, AlarmConfig *config, void *context) {
  config->tone = (uintptr_t)context;
  return true;
}

void alarm_set_tone(AlarmId id, AlarmTone tone) {
  prv_alarm_operation(id, prv_set_tone_op, (void *)(uintptr_t)tone, false);
}

// ----------------------------------------------------------------------------------------------
static bool prv_set_alarm_kind_op(AlarmId id, AlarmConfig *config, void *context) {
  AlarmKind type = (uintptr_t)context;
  switch (type) {
    case ALARM_KIND_EVERYDAY:
      config->kind = ALARM_KIND_EVERYDAY;
      const bool everyday[PBL_DAY_PER_WEEK] = {true, true, true, true, true, true, true};
      memcpy(&config->scheduled_days, everyday, sizeof(everyday));
      break;
    case ALARM_KIND_WEEKENDS:
      config->kind = ALARM_KIND_WEEKENDS;
      const bool weekends[PBL_DAY_PER_WEEK] = {true, false, false, false, false, false, true};
      memcpy(&config->scheduled_days, weekends, sizeof(weekends));
      break;
    case ALARM_KIND_WEEKDAYS:
      config->kind = ALARM_KIND_WEEKDAYS;
      const bool weekdays[PBL_DAY_PER_WEEK] = {false, true, true, true, true, true, false};
      memcpy(&config->scheduled_days, weekdays, sizeof(weekdays));
      break;
    case ALARM_KIND_JUST_ONCE:
      config->kind = ALARM_KIND_JUST_ONCE;
      const bool no_day[PBL_DAY_PER_WEEK] = {false, false, false, false, false, false, false};
      memcpy(&config->scheduled_days, no_day, sizeof(no_day));
      prv_set_day_for_just_once_alarm(config, config->hour, config->minute);
      break;
    default:
      return false;
  }
  return true;
}

void alarm_set_kind(AlarmId id, AlarmKind kind) {
  prv_alarm_operation(id, prv_set_alarm_kind_op, (void *)(uintptr_t)kind, true);
}

// ----------------------------------------------------------------------------------------------
static bool prv_set_alarm_custom_op(AlarmId id, AlarmConfig *config, void *context) {
  const bool (*scheduled_days)[PBL_DAY_PER_WEEK] = context;
  config->kind = ALARM_KIND_CUSTOM;
  memcpy(&config->scheduled_days, scheduled_days, sizeof(config->scheduled_days));
  return true;
}

void alarm_set_custom(AlarmId id, const bool scheduled_days[PBL_DAY_PER_WEEK]) {
  prv_alarm_operation(id, prv_set_alarm_custom_op, (void *)scheduled_days, true);
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_custom_days(AlarmId id, bool scheduled_days[PBL_DAY_PER_WEEK]) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }
  memcpy(scheduled_days, &config.scheduled_days, PBL_DAY_PER_WEEK);

cleanup:
  prv_file_close_and_unlock(&file);
  return rv;
}

// ----------------------------------------------------------------------------------------------
static void prv_cancel_snooze_for_disable(AlarmId id) {
  if (id != s_most_recent_alarm_id) {
    return;
  }

  PBL_LOG_DBG("Canceling snooze timer because alarm was disabled");
  // Harmless if the alarm is not currently snoozing - the snooze timer still exists to be stopped
  prv_clear_snooze_timer();
  s_most_recent_alarm_id = ALARM_INVALID_ID;
  s_smart_snooze_counter = 0;
}

// ----------------------------------------------------------------------------------------------
void alarm_set_enabled(AlarmId id, bool enable) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  if (!enable) {
    prv_cancel_snooze_for_disable(id);
    // Keep the fired-occurrence guard: toggling an alarm off and on must not re-fire an
    // occurrence that already fired.
    prv_invalidate_alarm_fire(id);
    prv_clear_missed_alarm(id);
  }

  prv_enable_alarm_config(&config, enable);
  prv_alarm_set_config(&file, id, &config);

cleanup:
  prv_file_close_and_unlock(&file);
}

// ----------------------------------------------------------------------------------------------
void alarm_delete(AlarmId id) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }

  prv_clear_missed_alarm(id);
  if (id == s_most_recent_alarm_id) {
    PBL_LOG_DBG("Canceling snooze timer on delete");
    prv_clear_snooze_timer();
    s_most_recent_alarm_id = ALARM_INVALID_ID;
    s_smart_snooze_counter = 0;
  }

  prv_invalidate_alarm_fire(id);
  prv_clear_fired_occurrence(id);
  AlarmStorageKey key = {.id = id, .type = ALARM_DATA_CONFIG};
  pbl_settings_delete(&file, &key, sizeof(key));
  prv_timeline_remove_alarm(&file, id);
  prv_reload_alarms(&file);

  prv_file_close_and_unlock(&file);
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_enabled(AlarmId id) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  bool is_enabled = false;
  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  is_enabled = !config.is_disabled;

cleanup:
  prv_file_close_and_unlock(&file);
  return is_enabled;
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_hours_minutes(AlarmId id, int *hour_out, int *minute_out) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  if (hour_out) {
    *hour_out = config.hour;
  }
  if (minute_out) {
    *minute_out = config.minute;
  }

cleanup:
  prv_file_close_and_unlock(&file);
  return rv;
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_next_enabled_alarm(time_t *next_alarm_time_out) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  const bool alarm_is_scheduled = s_next_alarm_time != 0;

  if (alarm_is_scheduled && next_alarm_time_out) {
    *next_alarm_time_out =
        s_next_alarm_time + (s_next_alarm_smart_window ? SMART_ALARM_RANGE_S : 0);
  }

  pbl_mutex_unlock(&s_mutex);

  return alarm_is_scheduled;
}

// ----------------------------------------------------------------------------------------------
bool alarm_is_next_enabled_alarm_smart(void) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);

  const bool alarm_is_scheduled = (s_next_alarm_time != 0);

  bool is_next_alarm_smart = false;
  if (alarm_is_scheduled) {
    is_next_alarm_smart = s_next_alarm_smart_window;
  }

  pbl_mutex_unlock(&s_mutex);

  return is_next_alarm_smart;
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_time_until(AlarmId id, time_t *time_out) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  bool alarm_is_scheduled = false;
  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  if (time_out) {
    const time_t now = pbl_realtime();
    AlarmOccurrence occurrence;
    prv_select_alarm_occurrence(id, &config, now, &occurrence);
    *time_out = occurrence.execute_time - now;
  }
  alarm_is_scheduled = true;

cleanup:
  prv_file_close_and_unlock(&file);
  return alarm_is_scheduled;
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_kind(AlarmId id, AlarmKind *kind_out) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  if (kind_out) {
    *kind_out = config.kind;
  }

cleanup:
  prv_file_close_and_unlock(&file);
  return rv;
}

// ----------------------------------------------------------------------------------------------
AlarmId alarm_get_most_recent_id(void) {
  return s_most_recent_alarm_id;
}

// ----------------------------------------------------------------------------------------------
bool alarm_get_info(AlarmId id, AlarmInfo *info_out) {
  if (!info_out) {
    return false;
  }

  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(&file, id, &config);
  if (!rv) {
    goto cleanup;
  }

  *info_out = (AlarmInfo){
    .hour = config.hour,
    .minute = config.minute,
    .kind = config.kind,
    .enabled = !config.is_disabled,
    .is_smart = config.is_smart,
    .sound_enabled = config.sound_enabled,
    .vibrate_enabled = !config.vibrate_disabled,
    .tone = config.tone,
    .scheduled_days = nullptr,
  };

cleanup:
  prv_file_close_and_unlock(&file);
  return rv;
}

static void prv_snooze_alarm(int snooze_delay_s, bool user_initiated) {
  prv_clear_snooze_timer();
  // Set the snooze kind before arming the timer.
  s_user_snoozed = user_initiated;
  PBL_LOG_INFO("Snoozing for %d minutes", snooze_delay_s / PBL_SEC_PER_MIN);
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_snooze_timer,
                              PBL_MSEC(snooze_delay_s * PBL_MSEC_PER_SEC));
}

// ----------------------------------------------------------------------------------------------
void alarm_set_snooze_alarm(void) {
  prv_snooze_alarm(s_snooze_delay_m * PBL_SEC_PER_MIN, true /* user_initiated */);
}

// ----------------------------------------------------------------------------------------------
uint16_t alarm_get_snooze_delay(void) {
  return s_snooze_delay_m;
}

// ----------------------------------------------------------------------------------------------
void alarm_set_snooze_delay(uint16_t delay_m) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }

  s_snooze_delay_m = delay_m;

  pbl_settings_set(&file, ALARM_PREF_KEY_SNOOZE_DELAY, strlen(ALARM_PREF_KEY_SNOOZE_DELAY),
                   &s_snooze_delay_m, sizeof(s_snooze_delay_m));

  prv_file_close_and_unlock(&file);
}

void alarm_dismiss_alarm(void) {
  prv_clear_snooze_timer();
}

// ----------------------------------------------------------------------------------------------
typedef struct {
  AlarmForEach cb;
  void *cb_data;
} ForEachAlarmItrData;

static bool alarm_for_each_itr(struct pbl_settings_file *file, struct pbl_settings_record *info,
                               void *context) {
  // check entry is valid
  if (info->val_len == 0 || info->key_len != sizeof(AlarmStorageKey)) {
    return true; // continue iterating
  }

  ForEachAlarmItrData *itr_data = (ForEachAlarmItrData *)context;

  AlarmStorageKey key;
  pbl_settings_read_key(file, (uint8_t *)&key, info->key_len);

  if (key.type != ALARM_DATA_CONFIG) {
    return true;
  }

  AlarmConfig config;
  bool rv = prv_alarm_get_config(file, key.id, &config);
  if (!rv) {
    return true;
  }

  AlarmInfo alarm_info = {
    .hour = config.hour,
    .minute = config.minute,
    .kind = config.kind,
    .enabled = !config.is_disabled,
    .is_smart = config.is_smart,
    .sound_enabled = config.sound_enabled,
    .vibrate_enabled = !config.vibrate_disabled,
    .tone = config.tone,
    .scheduled_days = &config.scheduled_days,
  };
  itr_data->cb(key.id, &alarm_info, itr_data->cb_data);

  return true;
}

void alarm_for_each(AlarmForEach cb, void *context) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }
  ForEachAlarmItrData itr_data = {
    .cb = cb,
    .cb_data = context,
  };
  pbl_settings_each(&file, alarm_for_each_itr, &itr_data);

  prv_file_close_and_unlock(&file);
}

// ----------------------------------------------------------------------------------------------
bool alarm_can_schedule(void) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return false;
  }

  bool rv = false;
  for (int i = 0; i < MAX_CONFIGURED_ALARMS; ++i) {
    AlarmConfig config;
    if (!prv_alarm_get_config(&file, i, &config)) {
      rv = true;
      break;
    }
  }

  prv_file_close_and_unlock(&file);
  return rv;
}

// ----------------------------------------------------------------------------------------------
void alarm_handle_clock_change(void) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }

  // Deferred until after the settings file is closed: prv_alarm_operation() opens it itself, and
  // opening it twice panics.
  AlarmId record_alarm_id = ALARM_INVALID_ID;

  // If there's an active alarm (e.g., currently snoozing), we need to handle it carefully.
  // For smart alarms near their deadline, we should trigger them before clearing state.
  if (s_most_recent_alarm_id != ALARM_INVALID_ID) {
    bool should_force_trigger = false;

    // Only consider force-triggering for smart alarms that are in their smart snooze window.
    // A user snooze is an explicit deadline and must survive any clock change; force-triggering
    // here would clear it and re-fire the alarm immediately.
    if (s_most_recent_alarm_config.is_smart && s_smart_snooze_counter > 0 && !s_user_snoozed) {
      // Check if we've used up most of our smart snooze attempts (within last 2 minutes)
      if (s_smart_snooze_counter >= (SMART_ALARM_MAX_SMART_SNOOZE - 2)) {
        should_force_trigger = true;
        PBL_LOG_INFO("Smart alarm %u at counter %d near deadline, forcing trigger",
                     s_most_recent_alarm_id, s_smart_snooze_counter);
      } else {
        // Also check if current time is within the smart alarm window past the deadline
        // This handles cases where counter < 28 but we're legitimately past the alarm time
        time_t now = pbl_realtime();
        struct pbl_tm now_tm;
        pbl_localtime_r(&now, &now_tm);
        int current_minutes = now_tm.tm_hour * 60 + now_tm.tm_min;
        int alarm_minutes =
            s_most_recent_alarm_config.hour * 60 + s_most_recent_alarm_config.minute;
        int time_diff_minutes = current_minutes - alarm_minutes;

        // Only trigger if we're 0-30 minutes past the alarm time (the smart window)
        // This prevents false triggers when time >= alarm but we're not in the window
        // (e.g., alarm at 5:35 AM, current time 11 PM would give time_diff = 1045 min)
        if (time_diff_minutes >= 0 && time_diff_minutes <= (SMART_ALARM_RANGE_S / 60)) {
          should_force_trigger = true;
          PBL_LOG_INFO("Smart alarm %u in window, %d min past deadline, forcing trigger",
                       s_most_recent_alarm_id, time_diff_minutes);
        }
      }
    }

    if (should_force_trigger) {
      // Trigger the alarm and let it follow its normal lifecycle (user can snooze/dismiss)
      prv_put_alarm_event();
      if (!s_most_recent_alarm_recorded) {
        s_most_recent_alarm_recorded = true;
        record_alarm_id = s_most_recent_alarm_id;
      }
      PBL_LOG_INFO("Clock change during alarm %u, triggered alarm", s_most_recent_alarm_id);
      // The alarm is now firing; stop the smart-snooze loop.
      prv_clear_snooze_timer();
      s_smart_snooze_counter = 0;
    } else {
      // Leave the in-flight smart-alarm snooze loop running. It is driven by
      // s_snooze_timer and counts toward the guaranteed fallback fire
      // (s_smart_snooze_counter >= SMART_ALARM_MAX_SMART_SNOOZE). A benign clock
      // change such as a periodic phone time sync must not cancel it, or the
      // smart alarm silently never goes off while basic alarms keep working.
      PBL_LOG_INFO("Clock change during alarm %u, leaving snooze loop intact",
                   s_most_recent_alarm_id);
    }
  }

  prv_refresh_just_once_alarm_days(&file);

  prv_reload_alarms(&file);

  prv_file_close_and_unlock(&file);

  if (record_alarm_id != ALARM_INVALID_ID) {
    prv_alarm_operation(record_alarm_id, prv_record_alarm_op, nullptr, false);
  }
}

// ----------------------------------------------------------------------------------------------
static void prv_reload_alarms_kernel_bg_callback(void *unused) {
  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }
  prv_reload_alarms(&file);
  prv_file_close_and_unlock(&file);
}

static EventServiceInfo s_language_change_event_info;
static EventServiceInfo s_activity_event_info;

static void prv_language_change_event_handler(PebbleEvent *event, void *context) {
  pbl_sys_work_call(prv_reload_alarms_kernel_bg_callback, nullptr);
}

static void prv_activity_event_handler(PebbleEvent *event, void *context) {
  if (event->activity_event.type == PebbleActivityEvent_TrackingStarted ||
      event->activity_event.type == PebbleActivityEvent_TrackingStopped) {
    pbl_sys_work_call(prv_reload_alarms_kernel_bg_callback, nullptr);
  }
}

// ----------------------------------------------------------------------------------------------
void alarm_init(void) {
  s_next_alarm_time = 0;
  s_next_alarm_smart_window = false;
  s_most_recent_alarm_id = ALARM_INVALID_ID;
  memset(s_fired_occurrences, 0, sizeof(s_fired_occurrences));
  s_most_recent_alarm_smart_window = false;
  s_missed_alarm_catchup_id = ALARM_INVALID_ID;
  s_missed_alarm_catchup_time = 0;
  s_missed_alarm_generation++;
  s_missed_alarm_id = ALARM_INVALID_ID;
  s_missed_alarm_time = 0;
  for (int i = 0; i < MAX_CONFIGURED_ALARMS; i++) {
    prv_invalidate_alarm_fire(i);
  }
  s_smart_snooze_counter = 0;

  s_language_change_event_info = (EventServiceInfo){
    .type = PEBBLE_LANGUAGE_CHANGE_EVENT,
    .handler = prv_language_change_event_handler,
  };
  event_service_client_subscribe(&s_language_change_event_info);

  s_activity_event_info = (EventServiceInfo){
    .type = PEBBLE_ACTIVITY_EVENT,
    .handler = prv_activity_event_handler,
  };
  event_service_client_subscribe(&s_activity_event_info);

  struct pbl_settings_file file;
  if (!prv_file_open_and_lock(&file)) {
    return;
  }

  uint16_t snooze_delay_value;
  if (pbl_settings_get(&file, ALARM_PREF_KEY_SNOOZE_DELAY, strlen(ALARM_PREF_KEY_SNOOZE_DELAY),
                       &snooze_delay_value, sizeof(snooze_delay_value)) == 0) {
    s_snooze_delay_m = snooze_delay_value;
  }

  AlarmArmedRecord armed;
  if (pbl_settings_get(&file, ALARM_PREF_KEY_ARMED, strlen(ALARM_PREF_KEY_ARMED), &armed,
                       sizeof(armed)) == 0) {
    s_armed_record = armed;

    // An alarm which was armed for a time we have already passed never got the chance to fire:
    // the watch was down (rebooting, or updating) when it was due.
    const time_t now = pbl_realtime();
    if (armed.time != 0 && armed.id != ALARM_INVALID_ID && now >= armed.time &&
        (now - armed.time) <= ALARM_MISSED_MAX_DELAY_S) {
      s_missed_alarm_id = armed.id;
      s_missed_alarm_time = armed.time;
      PBL_LOG_INFO("Alarm %d missed by %lds, firing once alarms are enabled", armed.id,
                   (long)(now - armed.time));
    }
  }

  // A "just once" alarm which was missed is still armed for its original weekday, a week out.
  prv_refresh_just_once_alarm_days(&file);

  prv_reload_alarms(&file);
  prv_file_close_and_unlock(&file);
}

// ----------------------------------------------------------------------------------------------
void alarm_service_enable_alarms(bool enable) {
  s_alarms_enabled = enable;

  if (!enable) {
    return;
  }

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  const AlarmId id = s_missed_alarm_id;
  const time_t missed_time = s_missed_alarm_time;
  if (id == ALARM_INVALID_ID) {
    pbl_mutex_unlock(&s_mutex);
    return;
  }
  s_missed_alarm_id = ALARM_INVALID_ID;
  s_missed_alarm_time = 0;

  if ((pbl_realtime() - missed_time) > ALARM_MISSED_MAX_DELAY_S) {
    pbl_mutex_unlock(&s_mutex);
    PBL_LOG_DBG("Missed alarm %d is too late to fire", id);
    return;
  }
  s_missed_alarm_catchup_id = id;
  s_missed_alarm_catchup_time = missed_time;
  s_missed_alarm_generation++;
  const uintptr_t generation = s_missed_alarm_generation;
  pbl_mutex_unlock(&s_mutex);

  pbl_sys_work_call(prv_missed_alarm_catchup_callback, (void *)generation);
}

// ----------------------------------------------------------------------------------------------
const char *alarm_get_string_for_kind(AlarmKind kind, bool all_caps) {
  const char *alarm_day_text = nullptr;
  switch (kind) {
    case ALARM_KIND_EVERYDAY:
      alarm_day_text = all_caps ?
                                /// A frequency option for alarms, i.e. the alarm would go off every
                                /// day. Respect capitalization!
                           pbl_i18n_noop("EVERY DAY")
                                :
                                /// A frequency option for alarms, i.e. the alarm would go off every
                                /// day. Respect capitalization!
                           pbl_i18n_noop("Every Day");
      break;
    case ALARM_KIND_WEEKDAYS:
      alarm_day_text = all_caps ?
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// every weekday. Respect capitalization!
                           pbl_i18n_noop("WEEKDAYS")
                                :
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// every weekday. Respect capitalization!
                           pbl_i18n_noop("Weekdays");
      break;
    case ALARM_KIND_WEEKENDS:
      alarm_day_text = all_caps ?
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// each day on the weekend. Respect capitalization!
                           pbl_i18n_noop("WEEKENDS")
                                :
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// each day on the weekend. Respect capitalization!
                           pbl_i18n_noop("Weekends");
      break;
    case ALARM_KIND_JUST_ONCE:

      alarm_day_text = all_caps ?
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// one time ever. Respect capitalization!
                           pbl_i18n_noop("ONCE")
                                :
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// one time ever. Respect capitalization!
                           pbl_i18n_noop("Once");
      break;
    case ALARM_KIND_CUSTOM:
      // TODO: Use selected days as the string
      alarm_day_text = all_caps ?
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// on a custom choice of days. Respect capitalization!
                           pbl_i18n_noop("CUSTOM")
                                :
                                /// A frequency option for alarms, i.e. the alarm would only go off
                                /// on a custom choice of days. Respect capitalization!
                           pbl_i18n_noop("Custom");
      break;
    default:
      alarm_day_text = "";
      break;
  }

  return alarm_day_text;
}

void alarm_get_string_for_custom(bool scheduled_days[PBL_DAY_PER_WEEK], char *alarm_day_text) {
  // 4 chars per day, 3 for letters and 1 for comma
  // max length = 7 days in a week * 4 chars per day = 28
  static const char *day_strings[7] = {pbl_i18n_noop("Sun"), pbl_i18n_noop("Mon"),
                                       pbl_i18n_noop("Tue"), pbl_i18n_noop("Wed"),
                                       pbl_i18n_noop("Thu"), pbl_i18n_noop("Fri"),
                                       pbl_i18n_noop("Sat")};
  static const char *full_day_strings[7] = {pbl_i18n_noop("Sundays"),   pbl_i18n_noop("Mondays"),
                                            pbl_i18n_noop("Tuesdays"),  pbl_i18n_noop("Wednesdays"),
                                            pbl_i18n_noop("Thursdays"), pbl_i18n_noop("Fridays"),
                                            pbl_i18n_noop("Saturdays")};

  uint8_t num_days_scheduled = 0, latest_day_scheduled = 0;
  // Monday should come first in the list
  for (unsigned int i = 1; i < 8; i++) {
    if (scheduled_days[i % 7] == true) {
      num_days_scheduled++;
      latest_day_scheduled = i % 7;
      const char *day_string = pbl_i18n_get(day_strings[i % 7], day_strings);
      strcat(alarm_day_text, day_string);
      strcat(alarm_day_text, ",");
      pbl_i18n_free(day_strings[i % 7], day_strings); // copied in by strcat. We can free it
    }
  }
  if (num_days_scheduled == 1) {
    // Write the full day string
    const char *full_day_string =
        pbl_i18n_get(full_day_strings[latest_day_scheduled], full_day_strings);
    strcpy(alarm_day_text, full_day_string);
    pbl_i18n_free(full_day_strings[latest_day_scheduled], full_day_strings); // copied in above.
  } else if (num_days_scheduled > 1) {
    // Write the shortened day strings, remove last ','
    alarm_day_text[strnlen(alarm_day_text, 28) - 1] = 0;
  }
}

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW)
#include <pbl/shell/shell.h>

static int prv_cmd_alarm(const struct pbl_shell *sh, size_t argc, char **argv) {
  prv_put_alarm_event();
  return 0;
}

PBL_SHELL_CMD_REGISTER(alarm, nullptr, "Fire an alarm event", prv_cmd_alarm);
#endif
