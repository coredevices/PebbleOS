/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "vibe_score_info.h"

#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "system/passert.h"
#include "util/size.h"

typedef enum AlertType {
  AlertType_Notifications = 1 << 0,
  AlertType_Calls = 1 << 1,
  AlertType_Alarms = 1 << 2,
  AlertType_AlarmsLPM = 1 << 3,
  AlertType_All = AlertType_Notifications | AlertType_Calls | AlertType_Alarms,
} AlertType;

typedef struct {
  const VibeScoreId id;
  const char *name;
  const int alert_types;
  const uint32_t resource_id;
} VibeScoreInfo;

#define VIBE_DEF(identifier, enum_name, name_str, alert_types_arg, res_id)\
  { .id = identifier, \
    .name = name_str, \
    .alert_types = alert_types_arg, \
    .resource_id = res_id },

static const VibeScoreInfo S_VIBE_MAP[] = {
#include "vibes.def"
};
#undef VIBE_DEF

#define VIBE_MAP_SIZE ARRAY_LENGTH(S_VIBE_MAP)

static const VibeScoreInfo *prv_vibe_score_find_info(VibeScoreId id) {
  for (unsigned int x = 0; x < VIBE_MAP_SIZE; x++) {
    if (S_VIBE_MAP[x].id == id) {
      return &S_VIBE_MAP[x];
    }
  }
  return NULL;
}

uint32_t vibe_score_info_get_resource_id(VibeScoreId id) {
  const VibeScoreInfo *info = prv_vibe_score_find_info(id);
  return info ? info->resource_id : RESOURCE_ID_INVALID;
}

const char *vibe_score_info_get_name(VibeScoreId id) {
  const VibeScoreInfo *info = prv_vibe_score_find_info(id);
  return info ? info->name : "";
}

static int prv_get_index(VibeScoreId id) {
  for (unsigned int x = 0; x < VIBE_MAP_SIZE; x++) {
    if (S_VIBE_MAP[x].id == id) {
      return x;
    }
  }
  return 0;
}

VibeScoreId vibe_score_info_cycle_next(VibeClient client, VibeScoreId curr_id) {
  AlertType alert_type;
  switch (client) {
    case VibeClient_Notifications: {
      alert_type = AlertType_Notifications;
      break;
    }
    case VibeClient_PhoneCalls: {
      alert_type = AlertType_Calls;
      break;
    }
    case VibeClient_Alarms: {
      alert_type = AlertType_Alarms;
      break;
    }
    default: {
      WTF;
    }
  }

  unsigned int currently_showing_index = prv_get_index(curr_id);
  unsigned int search_index = (currently_showing_index + 1) % VIBE_MAP_SIZE;

  while (search_index != currently_showing_index) {
    if ((S_VIBE_MAP[search_index].alert_types & alert_type) == alert_type) {
      return S_VIBE_MAP[search_index].id;
    }
    search_index = (search_index + 1) % VIBE_MAP_SIZE;
  }

  return curr_id;
}

bool vibe_score_info_is_valid(VibeScoreId id) {
  const VibeScoreInfo *info = prv_vibe_score_find_info(id);
  return (id != VibeScoreId_Invalid) &&
         info &&
         ((id == VibeScoreId_Disabled) || (info->resource_id != RESOURCE_ID_INVALID));
}
