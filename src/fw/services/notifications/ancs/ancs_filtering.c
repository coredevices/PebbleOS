/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/ancs/ancs_filtering.h"

#include "drivers/rtc.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/notifications/alerts_preferences.h"
#include "pbl/services/timeline/attributes_actions.h"
#include "system/logging.h"
#include "util/pstring.h"

#include <string.h>

typedef enum {
  FilteringMatchTypeText = 0,
  FilteringMatchTypeRegex = 1,
} FilteringMatchType;

typedef enum {
  FilteringMatchFieldAny = 0,
  FilteringMatchFieldTitle = 1,
  FilteringMatchFieldBody = 2,
} FilteringMatchField;

typedef struct PACKED {
  uint8_t count;
  uint8_t data[];
} FilteringRulesSerialized;

typedef struct PACKED {
  uint8_t match_type;
  uint8_t match_field;
  uint8_t case_sensitive;
  char pattern[];
} FilteringRuleSerialized;

static char prv_ascii_to_lower(char c) {
  if ((c >= 'A') && (c <= 'Z')) {
    return (char)(c + ('a' - 'A'));
  }
  return c;
}

static bool prv_match_contains(const char *haystack, size_t haystack_len, const char *needle,
                               size_t needle_len, bool case_sensitive) {
  if (needle_len == 0) {
    return true;
  }
  if (haystack_len < needle_len) {
    return false;
  }

  for (size_t i = 0; i <= (haystack_len - needle_len); i++) {
    bool matched = true;
    for (size_t j = 0; j < needle_len; j++) {
      char h = haystack[i + j];
      char n = needle[j];
      if (!case_sensitive) {
        h = prv_ascii_to_lower(h);
        n = prv_ascii_to_lower(n);
      }
      if (h != n) {
        matched = false;
        break;
      }
    }
    if (matched) {
      return true;
    }
  }
  return false;
}

static bool prv_match_rule(uint8_t match_type, uint8_t match_field, bool case_sensitive,
                           const char *pattern, size_t pattern_len, const char *title,
                           size_t title_len, const char *body, size_t body_len) {
  if (match_type == FilteringMatchTypeRegex) {
    // Regex support is not available in firmware at the moment.
    return false;
  }

  const bool match_title =
      ((match_field == FilteringMatchFieldTitle) || (match_field == FilteringMatchFieldAny));
  const bool match_body =
      ((match_field == FilteringMatchFieldBody) || (match_field == FilteringMatchFieldAny));

  return ((match_title && prv_match_contains(title, title_len, pattern, pattern_len, case_sensitive))
      || (match_body && prv_match_contains(body, body_len, pattern, pattern_len, case_sensitive)));
}

bool ancs_filtering_matches_rules(const iOSNotifPrefs *app_notif_prefs,
                                  const ANCSAttribute *title_attr,
                                  const ANCSAttribute *body_attr) {
  if (!app_notif_prefs) {
    return false;
  }

  StringList *rules = attribute_get_string_list(&app_notif_prefs->attr_list,
                                                AttributeIdNotificationFilteringRules);
  if (!rules || (rules->serialized_byte_length == 0)) {
    return false;
  }

  const char *title = "";
  size_t title_len = 0;
  char *allocated_title = NULL;
  if (title_attr && (title_attr->length > 0)) {
    allocated_title = kernel_zalloc_check(title_attr->length + 1);
    pstring_pstring16_to_string(&title_attr->pstr, allocated_title);
    title = allocated_title;
    title_len = strlen(title);
  }

  const char *body = "";
  size_t body_len = 0;
  char *allocated_body = NULL;
  if (body_attr && (body_attr->length > 0)) {
    allocated_body = kernel_zalloc_check(body_attr->length + 1);
    pstring_pstring16_to_string(&body_attr->pstr, allocated_body);
    body = allocated_body;
    body_len = strlen(body);
  }

  const FilteringRulesSerialized *serialized_rules = (const FilteringRulesSerialized *)rules->data;
  size_t remaining = rules->serialized_byte_length - 1;
  const uint8_t *cursor = serialized_rules->data;

  bool matched = false;
  for (uint8_t i = 0; i < serialized_rules->count; i++) {
    if (remaining < sizeof(FilteringRuleSerialized)) {
      break;
    }

    const FilteringRuleSerialized *rule = (const FilteringRuleSerialized *)cursor;
    cursor += sizeof(FilteringRuleSerialized);
    remaining -= sizeof(FilteringRuleSerialized);

    const char *pattern = (const char *)cursor;
    const char *terminator = memchr(cursor, '\0', remaining);
    if (!terminator) {
      break;
    }

    const size_t pattern_len = (size_t)(terminator - pattern);
    matched = prv_match_rule(rule->match_type, rule->match_field, (rule->case_sensitive != 0),
                             pattern, pattern_len, title, title_len, body, body_len);

    cursor = (const uint8_t *)(terminator + 1);
    remaining -= (pattern_len + 1);

    if (matched) {
      break;
    }
  }

  kernel_free(allocated_body);
  kernel_free(allocated_title);
  return matched;
}

void ancs_filtering_record_app(iOSNotifPrefs **notif_prefs,
                               const ANCSAttribute *app_id,
                               const ANCSAttribute *display_name,
                               const ANCSAttribute *title) {
  // When we receive a notification, information about the app that sent us the notification
  // is recorded in the notif_pref_db. We sync this DB with the phone which allows us to
  // do things like add non ANCS actions, or filter notifications by app

  // The "default" attributes are merged with any existing attributes. This makes it easy to add
  // new attributes in the future as well as support EMail / SMS apps which already have data
  // stored.

  iOSNotifPrefs *app_notif_prefs = *notif_prefs;
  const int num_existing_attribtues = app_notif_prefs ? app_notif_prefs->attr_list.num_attributes :
                                                        0;

  AttributeList new_attr_list;
  attribute_list_init_list(num_existing_attribtues, &new_attr_list);
  bool list_dirty = false;

  // Copy over all the existing attributes to our new list
  if (app_notif_prefs) {
    for (int i = 0; i < num_existing_attribtues; i++) {
      new_attr_list.attributes[i] = app_notif_prefs->attr_list.attributes[i];
    }
  }

  // The app name should be the display name
  // If there is no display name (Apple Pay) then fallback to the title
  const ANCSAttribute *app_name_attr = NULL;
  if (display_name && display_name->length > 0) {
    app_name_attr = display_name;
  } else if (title && title->length > 0) {
    app_name_attr = title;
  }

  char *app_name_buff = NULL;
  if (app_name_attr) {
    const char *existing_name = "";
    if (app_notif_prefs) {
      existing_name = attribute_get_string(&app_notif_prefs->attr_list, AttributeIdAppName, "");
    }

    if (!pstring_equal_cstring(&app_name_attr->pstr, existing_name)) {
      // If the existing name doesn't match our new name, update the name
      app_name_buff = kernel_zalloc_check(app_name_attr->length + 1);
      pstring_pstring16_to_string(&app_name_attr->pstr, app_name_buff);
      attribute_list_add_cstring(&new_attr_list, AttributeIdAppName, app_name_buff);
      list_dirty = true;
      PBL_LOG_INFO("Adding app name to app prefs: <%s>", app_name_buff);
    }
  }

  // Add the mute attribute if we don't have one already
  // Default the app to not muted
  const bool already_has_mute =
      app_notif_prefs && attribute_find(&app_notif_prefs->attr_list, AttributeIdMuteDayOfWeek);
  if (!already_has_mute) {
    attribute_list_add_uint8(&new_attr_list, AttributeIdMuteDayOfWeek, MuteBitfield_None);
    list_dirty = true;
  }

  // Add the mute expiration attribute if we don't have one already
  // Default to no expiration (0 means not muted by time)
  if (!attribute_find(&new_attr_list, AttributeIdMuteExpiration)) {
    uint32_t expiration_value = 0;
    if (app_notif_prefs) {
      expiration_value = attribute_get_uint32(&app_notif_prefs->attr_list, AttributeIdMuteExpiration, 0);
    }
    attribute_list_add_uint32(&new_attr_list, AttributeIdMuteExpiration, expiration_value);
    list_dirty = true;
  }

  StringList *rules_attr = NULL;
  if (app_notif_prefs) {
    rules_attr = attribute_get_string_list(&app_notif_prefs->attr_list,
                                           AttributeIdNotificationFilteringRules);
  }
  StringList *default_rules = NULL;
  if (!rules_attr) {
    default_rules = kernel_zalloc_check(sizeof(StringList) + sizeof(uint8_t));
    default_rules->serialized_byte_length = sizeof(uint8_t);
    default_rules->data[0] = 0; // zero filtering rules by default
    attribute_list_add_string_list(&new_attr_list, AttributeIdNotificationFilteringRules,
                                   default_rules);
    list_dirty = true;
  }

  // Add / update the "last seen" timestamp
  Attribute *last_updated = NULL;
  if (app_notif_prefs) {
    last_updated = attribute_find(&app_notif_prefs->attr_list, AttributeIdLastUpdated);
  }
  uint32_t now = rtc_get_time();
  // Only perform an update if there is no timestamp or the current timestamp is more than a day old
  if (!last_updated ||
      (last_updated && now > (last_updated->uint32 + SECONDS_PER_DAY))) {
    attribute_list_add_uint32(&new_attr_list, AttributeIdLastUpdated, now);
    list_dirty = true;
    PBL_LOG_INFO("Updating / adding timestamp to app prefs");
  }

  if (list_dirty) {
    // We don't change or add actions at this time
    TimelineItemActionGroup *new_action_group = NULL;
    if (app_notif_prefs) {
      new_action_group = &app_notif_prefs->action_group;
    }

    ios_notif_pref_db_store_prefs(app_id->value, app_id->length,
                                  &new_attr_list, new_action_group);

    // Update our copy of the prefs with the new data
    const size_t buf_size = attributes_actions_get_buffer_size(&new_attr_list, new_action_group);
    *notif_prefs = kernel_zalloc_check(sizeof(iOSNotifPrefs) + buf_size);
    uint8_t *buffer = (uint8_t*)*notif_prefs + sizeof(iOSNotifPrefs);

    attributes_actions_deep_copy(&new_attr_list, &(*notif_prefs)->attr_list, new_action_group,
                                 &(*notif_prefs)->action_group, buffer, buffer + buf_size);
    ios_notif_pref_db_free_prefs(app_notif_prefs);
  }


  kernel_free(app_name_buff);
  kernel_free(default_rules);
  attribute_list_destroy_list(&new_attr_list);
}

uint8_t ancs_filtering_get_mute_type(const iOSNotifPrefs *app_notif_prefs) {
  if (app_notif_prefs) {
    return attribute_get_uint8(&app_notif_prefs->attr_list,
                               AttributeIdMuteDayOfWeek,
                               MuteBitfield_None);
  }

  return MuteBitfield_None;
}

uint32_t ancs_filtering_get_mute_expiration(const iOSNotifPrefs *app_notif_prefs) {
  if (app_notif_prefs) {
    return attribute_get_uint32(&app_notif_prefs->attr_list,
                                 AttributeIdMuteExpiration, 0);
  }

  return 0;
}

bool ancs_filtering_is_muted(const iOSNotifPrefs *app_notif_prefs) {
  uint8_t mute_type = ancs_filtering_get_mute_type(app_notif_prefs);
  uint32_t expiration_ts = ancs_filtering_get_mute_expiration(app_notif_prefs);

  struct tm now_tm;
  time_t now = rtc_get_time();
  localtime_r(&now, &now_tm);

  return (mute_type & (1 << now_tm.tm_wday)) || (expiration_ts > (uint32_t)now);
}
