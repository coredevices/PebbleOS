/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notifications_history.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/timeline/attribute.h"
#include "pbl/services/timeline/timeline.h"
#include "pbl/util/string.h"

#include <string.h>

static int prv_compare_entries(const NotificationHistoryEntry *a,
                               const NotificationHistoryEntry *b) {
  if (a->timestamp != b->timestamp) {
    return (a->timestamp > b->timestamp) ? -1 : 1;
  }
  if (a->sequence != b->sequence) {
    return (a->sequence > b->sequence) ? -1 : 1;
  }
  return 0;
}

static const NotificationHistoryEntry *prv_row_entry(const NotificationHistoryRow *row) {
  return row->is_group ? &row->group.members->entry : &row->notification;
}

static int prv_row_comparator(void *a, void *b) {
  return prv_compare_entries(prv_row_entry(a), prv_row_entry(b));
}

static int prv_member_comparator(void *a, void *b) {
  return prv_compare_entries(&((NotificationHistoryMember *)a)->entry,
                             &((NotificationHistoryMember *)b)->entry);
}

static const char *prv_group_sender_for_item(const TimelineItem *item, char *buffer,
                                             size_t buffer_size) {
  static const Uuid s_android_notifications_source = UUID_NOTIFICATIONS_DATA_SOURCE;

  if (timeline_item_is_ancs_notif(item) ||
      !uuid_equal(&item->header.parent_id, &s_android_notifications_source)) {
    return NULL;
  }

  const char *sender = attribute_get_string(&item->attr_list, AttributeIdSender, "");
  if (IS_EMPTY_STRING(sender)) {
    sender = attribute_get_string(&item->attr_list, AttributeIdTitle, "");
  }

  strncpy(buffer, sender, buffer_size - 1);
  buffer[buffer_size - 1] = '\0';
  string_strip_trailing_whitespace(buffer, buffer);
  char *start = (char *)string_strip_leading_whitespace(buffer);

  return IS_EMPTY_STRING(start) ? NULL : start;
}

static NotificationHistoryRow *prv_find_group(NotificationHistory *history, const char *sender) {
  NotificationHistoryRow *row = history->rows;
  while (row) {
    if (row->is_group && (strcmp(row->group.sender, sender) == 0)) {
      return row;
    }
    row = (NotificationHistoryRow *)list_get_next(&row->node);
  }
  return NULL;
}

static void prv_insert_row_sorted(NotificationHistory *history, NotificationHistoryRow *row) {
  history->rows = (NotificationHistoryRow *)list_sorted_add((ListNode *)history->rows, &row->node,
                                                            prv_row_comparator, false);
}

static NotificationHistoryEntry prv_make_entry(NotificationHistory *history,
                                               const CommonTimelineItemHeader *header) {
  return (NotificationHistoryEntry){
    .id = header->id,
    .timestamp = header->timestamp,
    .sequence = history->next_sequence++,
  };
}

static NotificationHistoryRow *prv_create_individual_row(NotificationHistory *history,
                                                         const CommonTimelineItemHeader *header) {
  NotificationHistoryRow *row = app_malloc_check(sizeof(*row));
  *row = (NotificationHistoryRow){
    .notification = prv_make_entry(history, header),
  };
  list_init(&row->node);
  return row;
}

static NotificationHistoryMember *prv_create_member(NotificationHistory *history,
                                                    const CommonTimelineItemHeader *header) {
  NotificationHistoryMember *member = app_malloc_check(sizeof(*member));
  *member = (NotificationHistoryMember){
    .entry = prv_make_entry(history, header),
  };
  list_init(&member->node);
  return member;
}

static NotificationHistoryRow *prv_create_group(const char *sender) {
  NotificationHistoryRow *row = app_malloc_check(sizeof(*row));
  *row = (NotificationHistoryRow){
    .is_group = true,
  };
  list_init(&row->node);

  const size_t sender_size = strlen(sender) + 1;
  row->group.sender = app_malloc_check(sender_size);
  memcpy(row->group.sender, sender, sender_size);
  return row;
}

static void prv_free_group_members(NotificationHistoryMember *member) {
  while (member) {
    NotificationHistoryMember *next = (NotificationHistoryMember *)list_get_next(&member->node);
    app_free(member);
    member = next;
  }
}

static void prv_free_row(NotificationHistoryRow *row) {
  if (row->is_group) {
    prv_free_group_members(row->group.members);
    app_free(row->group.sender);
  }
  app_free(row);
}

void notifications_history_init(NotificationHistory *history, bool group_by_sender,
                                time_t grouping_cutoff) {
  *history = (NotificationHistory){
    .group_by_sender = group_by_sender,
    .grouping_cutoff = grouping_cutoff,
  };
}

void notifications_history_deinit(NotificationHistory *history) {
  NotificationHistoryRow *row = history->rows;
  while (row) {
    NotificationHistoryRow *next = (NotificationHistoryRow *)list_get_next(&row->node);
    prv_free_row(row);
    row = next;
  }
  history->rows = NULL;
}

void notifications_history_add_header(NotificationHistory *history,
                                      const CommonTimelineItemHeader *header) {
  NotificationHistoryRow *row = prv_create_individual_row(history, header);
  if (history->group_by_sender) {
    prv_insert_row_sorted(history, row);
  } else {
    history->rows = (NotificationHistoryRow *)list_prepend((ListNode *)history->rows, &row->node);
  }
}

void notifications_history_add_item(NotificationHistory *history, const TimelineItem *item) {
  char buffer[ATTRIBUTE_TITLE_MAX_LEN + 1];
  const char *sender = NULL;
  if (history->group_by_sender && (item->header.timestamp >= history->grouping_cutoff)) {
    sender = prv_group_sender_for_item(item, buffer, sizeof(buffer));
  }
  if (!sender) {
    notifications_history_add_header(history, &item->header);
    return;
  }

  NotificationHistoryRow *row = prv_find_group(history, sender);
  if (!row) {
    row = prv_create_group(sender);
  } else {
    list_remove(&row->node, (ListNode **)&history->rows, NULL);
  }

  NotificationHistoryMember *member = prv_create_member(history, &item->header);
  row->group.members = (NotificationHistoryMember *)list_sorted_add(
      (ListNode *)row->group.members, &member->node, prv_member_comparator, false);
  row->group.count++;
  prv_insert_row_sorted(history, row);
}

bool notifications_history_remove(NotificationHistory *history, const Uuid *id) {
  NotificationHistoryRow *row = history->rows;
  while (row) {
    if (!row->is_group) {
      if (uuid_equal(&row->notification.id, id)) {
        list_remove(&row->node, (ListNode **)&history->rows, NULL);
        prv_free_row(row);
        return true;
      }
    } else {
      NotificationHistoryMember *member = row->group.members;
      while (member && !uuid_equal(&member->entry.id, id)) {
        member = (NotificationHistoryMember *)list_get_next(&member->node);
      }
      if (member) {
        const bool removed_latest = (member == row->group.members);
        list_remove(&member->node, (ListNode **)&row->group.members, NULL);
        app_free(member);
        row->group.count--;

        if (row->group.count == 0) {
          list_remove(&row->node, (ListNode **)&history->rows, NULL);
          prv_free_row(row);
        } else if (removed_latest) {
          list_remove(&row->node, (ListNode **)&history->rows, NULL);
          prv_insert_row_sorted(history, row);
        }
        return true;
      }
    }
    row = (NotificationHistoryRow *)list_get_next(&row->node);
  }
  return false;
}

uint16_t notifications_history_get_row_count(const NotificationHistory *history) {
  return (uint16_t)list_count((ListNode *)history->rows);
}

NotificationHistoryRow *notifications_history_get_row(const NotificationHistory *history,
                                                      uint16_t index) {
  return (NotificationHistoryRow *)list_get_at((ListNode *)history->rows, index);
}

bool notifications_history_row_is_collapsed_group(const NotificationHistoryRow *row) {
  return row->is_group && (row->group.count > 1);
}

const Uuid *notifications_history_row_get_latest_id(const NotificationHistoryRow *row) {
  return &prv_row_entry(row)->id;
}
