/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "notifications_presented_list.h"

#include "util/list.h"
#include "kernel/pbl_malloc.h"

#include <stdbool.h>

// currently focused notification id
static NotifList *s_current_notif;

// List contains all currently presented notifications
// The UI can access only notifications of this list
static NotifList *s_presented_notifs;


static bool prv_filter_presented_notification_by_id(ListNode *found_node, void *data) {
  return uuid_equal(&((NotifList*)found_node)->notif.id, (Uuid *)data);
}

static NotifList *prv_find_listnode_for_notif(Uuid *id) {
  return (NotifList*) list_find((ListNode *) s_presented_notifs,
                                prv_filter_presented_notification_by_id, id);
}

Uuid *notifications_presented_list_first(void) {
  NotifList * node = (NotifList *)list_get_head((ListNode*)s_presented_notifs);
  return node ? &node->notif.id : NULL;
}

Uuid *notifications_presented_list_last(void) {
  NotifList * node = (NotifList *)list_get_tail((ListNode*)s_presented_notifs);
  return node ? &node->notif.id : NULL;
}

Uuid *notifications_presented_list_relative(Uuid *id, int offset) {
  NotifList *const start_node = prv_find_listnode_for_notif(id);
  NotifList *const end_node = (NotifList *)list_get_at((ListNode *)start_node, offset);

  return end_node ? &end_node->notif.id : NULL;
}

int notifications_presented_list_count(void) {
  return list_count((ListNode*)s_presented_notifs);
}

void notifications_presented_list_remove(Uuid *id) {
  NotifList *node = prv_find_listnode_for_notif(id);
  if (!node) {
    return;
  }

  if (node == s_current_notif) {
    NotifList *prev_node = (NotifList *)list_get_prev((ListNode *)s_current_notif);
    NotifList *next_node = (NotifList *)list_get_next((ListNode *)s_current_notif);

    // If a notification gets removed, we want to show an older notification next as we assume
    // that the user scrolls down in the list starting from the newest notification
    if (next_node) {
      s_current_notif = next_node;
    } else {
      s_current_notif = prev_node;
    }
  }

  list_remove((ListNode*)node, (ListNode**)&s_presented_notifs, NULL);
  task_free(node);
}

static NotifList* prv_add_notification_common(Uuid *id, NotificationType type) {
  notifications_presented_list_remove(id);

  NotifList *new_entry = task_malloc_check(sizeof(NotifList));
  list_init((ListNode*)new_entry);
  new_entry->notif.type = type;
  new_entry->notif.id = *id;

  return new_entry;
}

void notifications_presented_list_add(Uuid *id, NotificationType type) {
  NotifList *new_entry = prv_add_notification_common(id, type);
  s_presented_notifs = (NotifList *)
      list_prepend(&s_presented_notifs->list_node, &new_entry->list_node);
}

void notifications_presented_list_add_sorted(Uuid *id, NotificationType type,
                                             Comparator comparator, bool ascending) {
  NotifList *new_entry = prv_add_notification_common(id, type);
  s_presented_notifs = (NotifList *) list_sorted_add(&s_presented_notifs->list_node,
      &new_entry->list_node, comparator, ascending);
}

NotificationType notifications_presented_list_get_type(Uuid *id) {
  NotifList *node = prv_find_listnode_for_notif(id);
  if (!node) {
    return NotificationInvalid;
  }
  return node->notif.type;
}

bool notifications_presented_list_set_current(Uuid *id) {
  NotifList *node = prv_find_listnode_for_notif(id);
  if (!node) {
    return false;
  }

  s_current_notif = node;
  return true;
}

Uuid *notifications_presented_list_current(void) {
  if (!s_current_notif) {
    return NULL;
  }
  return &s_current_notif->notif.id;
}

Uuid *notifications_presented_list_next(void) {
  NotifList *next_node = (NotifList *)list_get_next((ListNode *)s_current_notif);
  if (!next_node) {
    return NULL;
  }
  return &next_node->notif.id;
}

int notifications_presented_list_current_idx(void) {
  Uuid *id = notifications_presented_list_current();
  if (uuid_is_invalid(id)) {
    return -1;
  }
  return list_count_to_head_from((ListNode*) prv_find_listnode_for_notif(id)) - 1;
}

void notifications_presented_list_init(void) {
  s_current_notif = NULL;
  s_presented_notifs = NULL;
}

void notifications_presented_list_deinit(NotificationListEachCallback callback, void *cb_data) {
  s_current_notif = NULL;

  while (s_presented_notifs) {
    NotifList *head = s_presented_notifs;
    list_remove((ListNode *)head, (ListNode **)&s_presented_notifs, NULL);

    if (callback) {
      callback(&head->notif.id, head->notif.type, cb_data);
    }

    task_free(head);
  }
}

void notifications_presented_list_each(NotificationListEachCallback callback, void *cb_data) {
  if (!callback) {
    return;
  }

  NotifList *itr = s_presented_notifs;
  while (itr) {
    NotifList *next = (NotifList *)itr->list_node.next;
    callback(&itr->notif.id, itr->notif.type, cb_data);
    itr = next;
  }
}
