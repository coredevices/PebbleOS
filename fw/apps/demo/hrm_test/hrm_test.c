/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/activity/activity.h>
#include <pbl/services/hrm/hrm_manager.h>
#include <pbl/services/hrm/hrm_manager_private.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/units.h>

#include <applib/app.h>
#include <applib/app_malloc.h>
#include <applib/app_timer.h>
#include <applib/persist.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/simple_menu_layer.h>
#include <applib/ui/text_layer.h>
#include <applib/ui/vibes.h>
#include <applib/ui/window.h>
#include <apps/system_app_ids.h>
#include <process_management/pebble_process_md.h>
#include <process_state/app_state/app_state.h>
#include <system/version.h>

#define MAX_METRICS 8
#define NO_VALUE    0xFFFFU
#define SETTLE_S    5U
#define LABEL_LEN   8
#define BODY_LEN    320
#define VALUE_LEN   8

#define KEY_LAST 1U
#define KEY_PREV 2U

typedef enum {
  MetricFirst,
  MetricHigh,
  MetricVerdict,
  MetricCount,
  MetricMean,
  MetricMin,
  MetricMax,
  MetricSd,
  MetricConf,
  MetricMaxConf,
  MetricHighPct,
  MetricOff,
  MetricLast,
  MetricLastConf,
  MetricInvalid,
  MetricPpiCount,
  MetricPpiMean,
  MetricRmssd,
  MetricPpiConf,
  MetricNone,
} Metric;

typedef struct {
  const char *label;
  bool tenths;
} MetricInfo;

static const MetricInfo s_metrics[] = {
  [MetricFirst] = {"First s"},    [MetricHigh] = {"High s"},      [MetricVerdict] = {"Off at s"},
  [MetricCount] = {"Readings"},   [MetricMean] = {"Mean", true},  [MetricMin] = {"Min"},
  [MetricMax] = {"Max"},          [MetricSd] = {"SD", true},      [MetricConf] = {"Conf"},
  [MetricMaxConf] = {"Max conf"}, [MetricHighPct] = {"High %"},   [MetricOff] = {"Off evts"},
  [MetricLast] = {"Last"},        [MetricLastConf] = {"Last cf"}, [MetricInvalid] = {"Invalid"},
  [MetricPpiCount] = {"Beats"},   [MetricPpiMean] = {"PPI ms"},   [MetricRmssd] = {"RMSSD"},
  [MetricPpiConf] = {"Conf"},
};

typedef struct {
  const char *name;
  const char *instructions;
  uint16_t duration_s;
  HRMFeature features;
  HRMActivityScene scene;
  bool cold_start;
  bool settle;
  bool stop_at_verdict;
  Metric metrics[MAX_METRICS];
} Step;

static const Step s_steps[] = {
  {
    .name = "Off wrist",
    .instructions = "Lay the watch on a table, sensor facing up.\n\nSelect: start (60 s)",
    .duration_s = 60U,
    .features = HRMFeature_BPM,
    .cold_start = true,
    .metrics = {MetricVerdict, MetricCount, MetricMaxConf, MetricLast, MetricNone},
  },
  {
    .name = "Put on",
    .instructions = "Wear it snugly and sit still.\n\nSelect: start (90 s)",
    .duration_s = 90U,
    .features = HRMFeature_BPM,
    .cold_start = true,
    .metrics = {MetricFirst, MetricHigh, MetricOff, MetricLast, MetricLastConf, MetricNone},
  },
  {
    .name = "Rest HR",
    .instructions = "Stay still and quiet, arm resting.\n\nSelect: start (90 s)",
    .duration_s = 90U,
    .features = HRMFeature_BPM,
    .settle = true,
    .metrics =
        {MetricMean, MetricMin, MetricMax, MetricSd, MetricConf, MetricHighPct, MetricOff,
         MetricNone},
  },
  {
    .name = "HRV",
    .instructions = "Stay still and quiet.\n\nSelect: start (60 s)",
    .duration_s = 60U,
    .features = HRMFeature_HRV,
    .settle = true,
    .metrics = {MetricPpiCount, MetricPpiMean, MetricRmssd, MetricPpiConf, MetricOff, MetricNone},
  },
  {
    .name = "SpO2",
    .instructions = "Stay still and quiet, arm resting.\n\nSelect: start (120 s)",
    .duration_s = 120U,
    .features = HRMFeature_SpO2,
    .settle = true,
    .metrics =
        {MetricFirst, MetricCount, MetricMean, MetricMin, MetricMax, MetricConf, MetricInvalid,
         MetricOff},
  },
  {
    .name = "Back to HR",
    .instructions = "Keep wearing it, still. Checks HR after SpO2.\n\nSelect: start (45 s)",
    .duration_s = 45U,
    .features = HRMFeature_BPM,
    .metrics = {MetricFirst, MetricMean, MetricConf, MetricOff, MetricNone},
  },
  {
    .name = "Walk",
    .instructions = "Walk at a steady pace. Workout HR path.\n\nSelect: start (60 s)\nDown: skip",
    .duration_s = 60U,
    .features = HRMFeature_BPM,
    .scene = HRMActivityScene_Walk,
    .settle = true,
    .metrics = {MetricMean, MetricMin, MetricMax, MetricConf, MetricHighPct, MetricOff, MetricNone},
  },
  {
    .name = "Take off",
    .instructions = "Take it off and lay it on the table, sensor up.\n\nSelect: start (up to 90 s)",
    .duration_s = 90U,
    .features = HRMFeature_BPM,
    .stop_at_verdict = true,
    .metrics = {MetricVerdict, MetricCount, MetricMaxConf, MetricNone},
  },
};

static const Step s_live[] = {
  {.name = "Live HR", .features = HRMFeature_BPM},
  {.name = "Live HRV", .features = HRMFeature_HRV},
  {.name = "Live SpO2", .features = HRMFeature_SpO2},
};

typedef struct {
  char label[LABEL_LEN];
  uint16_t values[ARRAY_LENGTH(s_steps)][MAX_METRICS];
} Run;

typedef struct {
  uint32_t count;
  uint32_t sum;
  uint32_t sum_sq;
  uint32_t conf_sum;
  uint32_t high;
  uint32_t off;
  uint32_t invalid;
  uint16_t first_s;
  uint16_t high_s;
  uint16_t verdict_s;
  uint8_t min;
  uint8_t max;
  uint8_t max_conf;
  uint8_t last;
  uint8_t last_conf;
  uint32_t ppi_count;
  uint32_t ppi_sum;
  uint32_t ppi_conf_sum;
  uint64_t diff_sq;
  uint16_t ppi_prev;
  uint8_t valid_level;
  uint8_t rejected;
  bool off_now;
} Stats;

typedef enum {
  ModeNoPrefs,
  ModeInstructions,
  ModeSettling,
  ModeMeasuring,
  ModeResults,
} Mode;

typedef struct {
  Window menu_window;
  SimpleMenuLayer menu;
  Window window;
  TextLayer title_layer;
  TextLayer body_layer;
  char title[32];
  char body[BODY_LEN];

  EventServiceInfo hrm_event_info;
  HRMSessionRef session;
  HRMFeature session_features;
  AppTimer *timer;

  Mode mode;
  const Step *cur;
  uint8_t step;
  uint8_t page;
  uint64_t start_ms;
  uint32_t settle_left;
  Stats stats;
  Run run;
  Run last;
  Run prev;
} AppData;

static uint16_t prv_elapsed_s(const AppData *data) {
  return (uint16_t)MAX((pbl_uptime_ms() - data->start_ms) / PBL_MSEC_PER_SEC, 1U);
}

static uint16_t prv_mean_tenths(uint32_t sum, uint32_t count) {
  return (count == 0U) ? NO_VALUE : (uint16_t)((10U * sum + count / 2U) / count);
}

static uint16_t prv_div(uint32_t a, uint32_t b) {
  return (b == 0U) ? NO_VALUE : (uint16_t)((a + b / 2U) / b);
}

static uint16_t prv_time(uint16_t s) {
  return (s == 0U) ? NO_VALUE : s;
}

static uint16_t prv_metric(const Stats *s, Metric metric) {
  switch (metric) {
    case MetricFirst:
      return prv_time(s->first_s);
    case MetricHigh:
      return prv_time(s->high_s);
    case MetricVerdict:
      return prv_time(s->verdict_s);
    case MetricCount:
      return (uint16_t)s->count;
    case MetricMean:
      return prv_mean_tenths(s->sum, s->count);
    case MetricMin:
      return (s->count == 0U) ? NO_VALUE : s->min;
    case MetricMax:
      return (s->count == 0U) ? NO_VALUE : s->max;
    case MetricSd: {
      if (s->count < 2U) {
        return NO_VALUE;
      }
      int64_t var = (int64_t)s->count * s->sum_sq - (int64_t)s->sum * s->sum;
      return (uint16_t)(integer_sqrt(100 * var) / (int32_t)s->count);
    }
    case MetricConf:
      return prv_div(s->conf_sum, s->count);
    case MetricMaxConf:
      return (s->count == 0U) ? NO_VALUE : s->max_conf;
    case MetricHighPct:
      return prv_div(100U * s->high, s->count);
    case MetricOff:
      return (uint16_t)s->off;
    case MetricLast:
      return (s->count == 0U) ? NO_VALUE : s->last;
    case MetricLastConf:
      return (s->count == 0U) ? NO_VALUE : s->last_conf;
    case MetricInvalid:
      return (uint16_t)s->invalid;
    case MetricPpiCount:
      return (uint16_t)s->ppi_count;
    case MetricPpiMean:
      return prv_div(s->ppi_sum, s->ppi_count);
    case MetricRmssd:
      return (s->ppi_count < 2U)
                 ? NO_VALUE
                 : (uint16_t)integer_sqrt((int64_t)(s->diff_sq / (s->ppi_count - 1U)));
    case MetricPpiConf:
      return prv_div(s->ppi_conf_sum, s->ppi_count);
    case MetricNone:
    default:
      return NO_VALUE;
  }
}

static void prv_format_value(char *buf, size_t len, Metric metric, uint16_t value) {
  if (value == NO_VALUE) {
    snprintf(buf, len, "--");
  } else if (s_metrics[metric].tenths) {
    snprintf(buf, len, "%u.%u", value / 10U, value % 10U);
  } else {
    snprintf(buf, len, "%u", value);
  }
}

static bool prv_is_measuring(const AppData *data) {
  return data->mode == ModeMeasuring;
}

static void prv_add_reading(AppData *data, uint8_t value, uint8_t confidence, bool off_wrist) {
  Stats *s = &data->stats;
  const uint16_t now_s = prv_elapsed_s(data);

  s->off_now = off_wrist;

  if (off_wrist) {
    s->off++;
    if (s->verdict_s == 0U) {
      s->verdict_s = now_s;
    }
    return;
  }

  if (value == 0U) {
    return;
  }

  if (s->verdict_s != 0U) {
    /* A reading with no off-wrist flag after the verdict means it was withdrawn. */
    s->verdict_s = 0U;
  }

  if (s->count == 0U) {
    s->first_s = now_s;
    s->min = value;
    s->max = value;
  }

  s->count++;
  s->sum += value;
  s->sum_sq += (uint32_t)value * value;
  s->conf_sum += confidence;
  s->min = MIN(s->min, value);
  s->max = MAX(s->max, value);
  s->max_conf = MAX(s->max_conf, confidence);
  s->last = value;
  s->last_conf = confidence;

  if (confidence >= HRM_CONFIDENCE_HIGH) {
    s->high++;
    if (s->high_s == 0U) {
      s->high_s = now_s;
    }
  }
}

static void prv_add_ppi(AppData *data, HRMHRVData hrv) {
  Stats *s = &data->stats;

  s->off_now = hrv.off_wrist;

  if (hrv.off_wrist) {
    s->off++;
    return;
  }

  if (hrv.ppi_ms == 0U) {
    return;
  }

  if (s->ppi_prev != 0U) {
    int32_t diff = (int32_t)hrv.ppi_ms - s->ppi_prev;
    s->diff_sq += (uint64_t)(diff * diff);
  }

  s->ppi_prev = hrv.ppi_ms;
  s->ppi_count++;
  s->ppi_sum += hrv.ppi_ms;
  s->ppi_conf_sum += hrv.confidence;
}

static void prv_render(AppData *data);
static void prv_finish_step(AppData *data, bool record);

static void prv_handle_hrm(PebbleEvent *e, void *context) {
  AppData *data = app_state_get_user_data();
  const PebbleHRMEvent *hrm = &e->hrm;

  if (hrm->event_type == HRMEvent_SubscriptionExpiring) {
    data->session = sys_hrm_manager_app_subscribe(APP_ID_HRM_TEST, 1U, PBL_SEC_PER_HOUR,
                                                  data->session_features);
    return;
  }

  if (!prv_is_measuring(data)) {
    return;
  }

  if (hrm->event_type == HRMEvent_BPM) {
    prv_add_reading(data, hrm->bpm.bpm, hrm->bpm.confidence, hrm->bpm.off_wrist);
  } else if (hrm->event_type == HRMEvent_HRV) {
    prv_add_ppi(data, hrm->hrv);
  } else if (hrm->event_type == HRMEvent_SpO2) {
    data->stats.valid_level = hrm->spo2.valid_level;
    if (hrm->spo2.invalid) {
      data->stats.invalid++;
      data->stats.rejected = hrm->spo2.percent;
      data->stats.off_now = hrm->spo2.off_wrist;
    } else {
      prv_add_reading(data, hrm->spo2.percent, hrm->spo2.confidence, hrm->spo2.off_wrist);
    }
  }

  if (data->cur->stop_at_verdict && (data->stats.verdict_s != 0U)) {
    prv_finish_step(data, true);
  }
}

static void prv_stop_session(AppData *data) {
  if (data->session != HRM_INVALID_SESSION_REF) {
    sys_hrm_manager_unsubscribe(data->session);
    data->session = HRM_INVALID_SESSION_REF;
  }
}

static void prv_begin_measuring(AppData *data) {
  data->stats = (Stats){};
  data->start_ms = pbl_uptime_ms();
  data->mode = ModeMeasuring;
}

static void prv_tick(void *context) {
  AppData *data = context;

  data->timer = nullptr;

  if (data->mode == ModeSettling) {
    if (--data->settle_left == 0U) {
      prv_begin_measuring(data);
    }
  } else if (data->mode == ModeMeasuring) {
    if ((data->cur->duration_s != 0U) &&
        (((pbl_uptime_ms() - data->start_ms) / PBL_MSEC_PER_SEC) >= data->cur->duration_s)) {
      prv_finish_step(data, true);
      return;
    }
  } else {
    return;
  }

  prv_render(data);
  data->timer = app_timer_register(PBL_MSEC_PER_SEC, prv_tick, data);
}

static void prv_start_step(AppData *data, const Step *step) {
  data->cur = step;

  /* A foreground subscription bypasses the per-feature prefs, but not all of them being off. */
  if (!activity_prefs_heart_rate_is_enabled() && !activity_prefs_spo2_is_enabled()) {
    data->mode = ModeNoPrefs;
    prv_render(data);
    return;
  }

  if (step->cold_start || (data->session_features != step->features)) {
    prv_stop_session(data);
  }

  hrm_manager_set_activity_scene(step->scene);

  if (data->session == HRM_INVALID_SESSION_REF) {
    data->session =
        sys_hrm_manager_app_subscribe(APP_ID_HRM_TEST, 1U, PBL_SEC_PER_HOUR, step->features);
    data->session_features = step->features;
  }

  if (step->settle) {
    data->mode = ModeSettling;
    data->settle_left = SETTLE_S;
  } else {
    prv_begin_measuring(data);
  }

  prv_render(data);
  data->timer = app_timer_register(PBL_MSEC_PER_SEC, prv_tick, data);
}

static void prv_log_results(const AppData *data) {
  char value[VALUE_LEN];
  char line[40];

  PBL_LOG_INFO("HRMT run %s", data->last.label);

  for (uint8_t i = 0U; i < ARRAY_LENGTH(s_steps); i++) {
    for (uint8_t m = 0U; m < MAX_METRICS; m++) {
      const Metric metric = s_steps[i].metrics[m];

      if (metric == MetricNone) {
        break;
      }

      prv_format_value(value, sizeof(value), metric, data->last.values[i][m]);
      snprintf(line, sizeof(line), "%s: %s: %s", s_steps[i].name, s_metrics[metric].label, value);
      PBL_LOG_INFO("HRMT %s", line);
    }
  }
}

static void prv_load_runs(AppData *data) {
  if (persist_read_data(KEY_LAST, &data->last, sizeof(data->last)) != (int)sizeof(data->last)) {
    memset(&data->last, 0xFF, sizeof(data->last));
    strncpy(data->last.label, "none", LABEL_LEN);
  }

  if (persist_read_data(KEY_PREV, &data->prev, sizeof(data->prev)) != (int)sizeof(data->prev)) {
    memset(&data->prev, 0xFF, sizeof(data->prev));
    strncpy(data->prev.label, "none", LABEL_LEN);
  }
}

static void prv_save_run(AppData *data) {
  data->prev = data->last;
  data->last = data->run;
  (void)persist_write_data(KEY_PREV, &data->prev, sizeof(data->prev));
  (void)persist_write_data(KEY_LAST, &data->last, sizeof(data->last));
}

static void prv_enter_results(AppData *data) {
  prv_stop_session(data);
  hrm_manager_set_activity_scene(HRMActivityScene_Default);
  data->mode = ModeResults;
  data->page = 0U;
  prv_render(data);
}

static void prv_finish_step(AppData *data, bool record) {
  if (data->timer != nullptr) {
    app_timer_cancel(data->timer);
    data->timer = nullptr;
  }

  if (record) {
    const Step *step = &s_steps[data->step];

    for (uint8_t m = 0U; m < MAX_METRICS; m++) {
      data->run.values[data->step][m] =
          (step->metrics[m] == MetricNone) ? NO_VALUE : prv_metric(&data->stats, step->metrics[m]);
    }
  }

  data->mode = ModeInstructions;

  if (++data->step >= ARRAY_LENGTH(s_steps)) {
    data->step = 0U;
    data->cur = &s_steps[0];
    prv_save_run(data);
    prv_log_results(data);
    prv_enter_results(data);
    vibes_double_pulse();
    return;
  }

  data->cur = &s_steps[data->step];
  vibes_short_pulse();
  prv_render(data);
}

static void prv_render_status(AppData *data) {
  const Step *step = data->cur;
  const Stats *s = &data->stats;

  if (data->mode == ModeSettling) {
    snprintf(data->body, sizeof(data->body), "Settle: hold still\n\nStarting in %" PRIu32 " s",
             data->settle_left);
    return;
  }

  const uint32_t elapsed = (uint32_t)((pbl_uptime_ms() - data->start_ms) / PBL_MSEC_PER_SEC);
  char head[24];
  size_t pos;

  if (s->off_now) {
    snprintf(head, sizeof(head), "Off wrist");
  } else if (step->duration_s == 0U) {
    snprintf(head, sizeof(head), "%" PRIu32 " s", elapsed);
  } else {
    snprintf(head, sizeof(head), "%" PRIu32 " s left",
             (step->duration_s > elapsed) ? (step->duration_s - elapsed) : 0U);
  }

  if (step->features == HRMFeature_HRV) {
    pos = (size_t)snprintf(data->body, sizeof(data->body),
                           "%s\n\nBeats: %" PRIu32 "\nLast PPI: %u ms", head, s->ppi_count,
                           s->ppi_prev);
  } else {
    pos = (size_t)snprintf(
        data->body, sizeof(data->body), "%s\n\n%s: %u\nConfidence: %u\nReadings: %" PRIu32, head,
        (step->features == HRMFeature_SpO2) ? "SpO2" : "BPM", s->last, s->last_conf, s->count);
  }

  if (step->features == HRMFeature_SpO2) {
    pos += (size_t)snprintf(&data->body[pos], sizeof(data->body) - pos,
                            "\nValid lvl: %u\nRejected: %" PRIu32 " (%u)", s->valid_level,
                            s->invalid, s->rejected);
  }

  snprintf(&data->body[pos], sizeof(data->body) - pos, "\nOff events: %" PRIu32, s->off);
}

static void prv_render_results(AppData *data) {
  const Step *step = &s_steps[data->page];
  size_t pos;

  snprintf(data->title, sizeof(data->title), "%s %u/%u", step->name, data->page + 1U,
           (unsigned)ARRAY_LENGTH(s_steps));
  pos = (size_t)snprintf(data->body, sizeof(data->body), "A %s | B %s\n", data->prev.label,
                         data->last.label);

  for (uint8_t m = 0U; (m < MAX_METRICS) && (step->metrics[m] != MetricNone); m++) {
    const Metric metric = step->metrics[m];
    char a[VALUE_LEN];
    char b[VALUE_LEN];

    prv_format_value(a, sizeof(a), metric, data->prev.values[data->page][m]);
    prv_format_value(b, sizeof(b), metric, data->last.values[data->page][m]);
    pos += (size_t)snprintf(&data->body[pos], sizeof(data->body) - pos, "%s: %s | %s\n",
                            s_metrics[metric].label, a, b);
  }
}

static void prv_render(AppData *data) {
  const Step *step = data->cur;

  if (step->duration_s == 0U) {
    snprintf(data->title, sizeof(data->title), "%s", step->name);
  } else {
    snprintf(data->title, sizeof(data->title), "%u/%u %s", data->step + 1U,
             (unsigned)ARRAY_LENGTH(s_steps), step->name);
  }

  switch (data->mode) {
    case ModeNoPrefs:
      snprintf(data->body, sizeof(data->body), "Enable heart rate or SpO2 monitoring");
      break;
    case ModeInstructions:
      snprintf(data->body, sizeof(data->body), "%s", step->instructions);
      break;
    case ModeSettling:
    case ModeMeasuring:
      prv_render_status(data);
      break;
    case ModeResults:
      prv_render_results(data);
      break;
  }

  text_layer_set_text(&data->title_layer, data->title);
  text_layer_set_text(&data->body_layer, data->body);
  layer_mark_dirty(&data->window.layer);
}

static void prv_cancel_timer(AppData *data) {
  if (data->timer != nullptr) {
    app_timer_cancel(data->timer);
    data->timer = nullptr;
  }
}

static void prv_leave(AppData *data) {
  prv_cancel_timer(data);
  prv_stop_session(data);
  hrm_manager_set_activity_scene(HRMActivityScene_Default);
  data->mode = ModeInstructions;
  app_window_stack_pop(true);
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  AppData *data = context;

  if (data->mode == ModeInstructions) {
    prv_start_step(data, data->cur);
  } else if (data->mode == ModeResults) {
    prv_log_results(data);
    vibes_short_pulse();
  }
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  AppData *data = context;

  if (data->mode == ModeResults) {
    data->page = (data->page + 1U) % ARRAY_LENGTH(s_steps);
    prv_render(data);
  } else if ((data->mode != ModeNoPrefs) && (data->cur->duration_s != 0U)) {
    /* Skip the step; it keeps its empty results. */
    prv_finish_step(data, false);
  }
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  AppData *data = context;

  if (data->mode == ModeResults) {
    data->page = (data->page + ARRAY_LENGTH(s_steps) - 1U) % ARRAY_LENGTH(s_steps);
    prv_render(data);
  }
}

static void prv_back_click(ClickRecognizerRef recognizer, void *context) {
  AppData *data = context;
  const bool running = (data->mode == ModeSettling) || (data->mode == ModeMeasuring);

  if (running && (data->cur->duration_s != 0U)) {
    prv_cancel_timer(data);
    data->mode = ModeInstructions;
    prv_render(data);
  } else {
    prv_leave(data);
  }
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click);
}

static void prv_reset_run(AppData *data) {
  FirmwareMetadata metadata;

  memset(&data->run, 0xFF, sizeof(data->run));
  if (version_copy_running_fw_metadata(&metadata)) {
    strncpy(data->run.label, metadata.version_short, LABEL_LEN - 1);
    data->run.label[LABEL_LEN - 1] = '\0';
  } else {
    strncpy(data->run.label, "?", LABEL_LEN);
  }
}

static void prv_guided_cb(int index, void *context) {
  AppData *data = context;

  prv_reset_run(data);
  data->step = 0U;
  data->cur = &s_steps[0];
  data->mode = ModeInstructions;
  prv_render(data);
  app_window_stack_push(&data->window, true);
}

static void prv_live_cb(int index, void *context) {
  AppData *data = context;

  app_window_stack_push(&data->window, true);
  prv_start_step(data, &s_live[index - 1]);
}

static void prv_results_cb(int index, void *context) {
  AppData *data = context;

  data->cur = &s_steps[0];
  prv_enter_results(data);
  app_window_stack_push(&data->window, true);
}

static const SimpleMenuItem s_menu_items[] = {
  {"Guided test", "A/B, step by step", nullptr, prv_guided_cb},
  {"Live HR", nullptr, nullptr, prv_live_cb},
  {"Live HRV", nullptr, nullptr, prv_live_cb},
  {"Live SpO2", nullptr, nullptr, prv_live_cb},
  {"Results", "Last two runs", nullptr, prv_results_cb},
};

static const SimpleMenuSection s_menu_sections[] = {
  {.items = s_menu_items, .num_items = ARRAY_LENGTH(s_menu_items)},
};

static void prv_init(void) {
  AppData *data = app_zalloc_check(sizeof(*data));

  app_state_set_user_data(data);
  data->session = HRM_INVALID_SESSION_REF;

  prv_load_runs(data);

  Window *window = &data->window;
  window_init(window, "HRM Test");
  window_set_fullscreen(window, true);
  window_set_click_config_provider_with_context(window, prv_click_config, data);

  GRect bounds = window->layer.bounds;

  text_layer_init(&data->title_layer, &GRect(4, 2, bounds.size.w - 8, 28));
  text_layer_set_font(&data->title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(&data->title_layer, GTextAlignmentCenter);
  layer_add_child(&window->layer, &data->title_layer.layer);

  text_layer_init(&data->body_layer, &GRect(6, 32, bounds.size.w - 12, bounds.size.h - 34));
  text_layer_set_font(&data->body_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(&window->layer, &data->body_layer.layer);

  data->hrm_event_info = (EventServiceInfo){
    .type = PEBBLE_HRM_EVENT,
    .handler = prv_handle_hrm,
  };
  event_service_client_subscribe(&data->hrm_event_info);

  Window *menu_window = &data->menu_window;
  window_init(menu_window, "HRM Test menu");
  simple_menu_layer_init(&data->menu, &(GRect){{0, 0}, menu_window->layer.frame.size}, menu_window,
                         s_menu_sections, ARRAY_LENGTH(s_menu_sections), data);
  layer_add_child(&menu_window->layer, simple_menu_layer_get_layer(&data->menu));
  app_window_stack_push(menu_window, true);
}

static void prv_deinit(void) {
  AppData *data = app_state_get_user_data();

  event_service_client_unsubscribe(&data->hrm_event_info);
  prv_stop_session(data);
  hrm_manager_set_activity_scene(HRMActivityScene_Default);
  simple_menu_layer_deinit(&data->menu);
}

static void prv_main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

const PebbleProcessMd *hrm_test_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .name = "HRM Test",
    .common.uuid =
        {0x3e, 0x91, 0x57, 0x0a, 0x6b, 0x2c, 0x4d, 0x18, 0xa4, 0x7f, 0x0c, 0xd2, 0x96, 0xe1, 0x3b,
         0x58},
    .common.main_func = &prv_main,
  };

  return sys_hrm_manager_is_hrm_present() ? (const PebbleProcessMd *)&s_app_info : nullptr;
}
