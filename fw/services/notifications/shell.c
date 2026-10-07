/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <pbl/shell/shell.h>

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"
#include "pbl/services/notifications/ancs/ancs_notifications.h"
#include "pbl/services/notifications/notification_storage.h"
#include "pbl/services/notifications/notifications.h"
#include "pbl/services/timeline/item.h"
#include "pbl/util/uuid.h"
#include <pbl/drivers/rtc.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#ifdef CONFIG_TOUCH
static int prv_cmd_test(const struct pbl_shell *sh, size_t argc, char **argv) {
  AttributeList attr_list = {};
  attribute_list_add_cstring(&attr_list, AttributeIdTitle, "Touch Test");
  attribute_list_add_cstring(
      &attr_list, AttributeIdBody,
      "Swipe up/down to scroll this body. Line 2. Line 3. Line 4. Line 5. Line 6. Line 7. Line 8. "
      "Line 9. Line 10. Line 11. Line 12. Line 13. Line 14. Swipe left=BACK, right=SELECT.");

  AttributeList dismiss_attr = {};
  attribute_list_add_cstring(&dismiss_attr, AttributeIdTitle, "Dismiss");
  TimelineItemActionGroup action_group = {
    .num_actions = 1,
    .actions = (TimelineItemAction[]){
      {.id = 0, .type = TimelineItemActionTypeDismiss, .attr_list = dismiss_attr},
    },
  };

  TimelineItem *item =
      timeline_item_create_with_attributes(rtc_get_time(), 0, TimelineItemTypeNotification,
                                           LayoutIdNotification, &attr_list, &action_group);
  attribute_list_destroy_list(&attr_list);
  attribute_list_destroy_list(&dismiss_attr);
  notifications_add_notification(item);
  timeline_item_destroy(item);

  pbl_shell_print(sh, "test notification added");
  return 0;
}
#endif

static int prv_parse_ancs_uid(const struct pbl_shell *sh, const char *arg, uint32_t *uid_out) {
  unsigned long uid;
  if (pbl_shell_strtoul(arg, &uid) != 0 || uid > UINT32_MAX) {
    pbl_shell_error(sh, "invalid ANCS uid '%s'", arg);
    return -EINVAL;
  }
  *uid_out = uid;
  return 0;
}

// Stands in for an iOS notification, since the emulator has no ANCS link. It carries the same
// Dismiss action an ANCS notification gets, so dismissing it on the watch takes the ANCS path.
static int prv_cmd_ancs_add(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t uid;
  int rv = prv_parse_ancs_uid(sh, argv[1], &uid);
  if (rv != 0) {
    return rv;
  }

  char title[24];
  snprintf(title, sizeof(title), "iOS %" PRIu32, uid);
  AttributeList attr_list = {};
  attribute_list_add_cstring(&attr_list, AttributeIdTitle, title);
  attribute_list_add_cstring(&attr_list, AttributeIdBody, "Test notification from an iPhone");

  AttributeList dismiss_attr = {};
  attribute_list_add_uint8(&dismiss_attr, AttributeIdAncsAction, ActionIDNegative);
  attribute_list_add_cstring(&dismiss_attr, AttributeIdTitle, "Dismiss");
  TimelineItemActionGroup action_group = {
    .num_actions = 1,
    .actions = (TimelineItemAction[]){
      {.id = 0, .type = TimelineItemActionTypeAncsNegative, .attr_list = dismiss_attr},
    },
  };

  TimelineItem *item =
      timeline_item_create_with_attributes(rtc_get_time(), 0, TimelineItemTypeNotification,
                                           LayoutIdNotification, &attr_list, &action_group);
  attribute_list_destroy_list(&attr_list);
  attribute_list_destroy_list(&dismiss_attr);
  item->header.ancs_notif = true;
  item->header.ancs_uid = uid;
  notifications_add_notification(item);

  char uuid_string[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&item->header.id, uuid_string);
  timeline_item_destroy(item);

  pbl_shell_print(sh, "ancs %" PRIu32 " added as %s", uid, uuid_string);
  return 0;
}

// Runs the same handler as an iPhone clearing the notification from Notification Center
static int prv_cmd_ancs_remove(const struct pbl_shell *sh, size_t argc, char **argv) {
  uint32_t uid;
  int rv = prv_parse_ancs_uid(sh, argv[1], &uid);
  if (rv != 0) {
    return rv;
  }

  ancs_notifications_handle_notification_removed(uid, ANCSProperty_iOS9);
  pbl_shell_print(sh, "ancs %" PRIu32 " removed from Notification Center", uid);
  return 0;
}

static bool prv_list_callback(void *data, SerializedTimelineItemHeader *header) {
  const struct pbl_shell *sh = data;
  const CommonTimelineItemHeader *common = &header->common;
  char uuid_string[UUID_STRING_BUFFER_LENGTH];
  uuid_to_string(&common->id, uuid_string);
  pbl_shell_print(sh, "%s status=0x%02x%s%s%s%s ancs=%" PRIu32, uuid_string, common->status,
                  common->read ? " read" : "", common->actioned ? " actioned" : "",
                  common->dismissed ? " dismissed" : "", common->deleted ? " deleted" : "",
                  common->ancs_notif ? common->ancs_uid : 0);
  return true;
}

static int prv_cmd_list(const struct pbl_shell *sh, size_t argc, char **argv) {
  notification_storage_iterate(prv_list_callback, (void *)sh);
  return 0;
}

static const struct pbl_shell_cmd sub_notif_ancs[] = {
  PBL_SHELL_CMD_ARG(add, NULL, "Add a stand-in iOS notification <uid>", prv_cmd_ancs_add, 2, 0),
  PBL_SHELL_CMD_ARG(remove, NULL, "Clear iOS notification <uid> as Notification Center would",
                    prv_cmd_ancs_remove, 2, 0),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_SUBCMD_SET_CREATE(sub_notif);
PBL_SHELL_CMD_REGISTER(notif, sub_notif, "Notifications", NULL);

#ifdef CONFIG_TOUCH
PBL_SHELL_SUBCMD_ADD(sub_notif, test, NULL, "Add a long scrollable test notification", prv_cmd_test,
                     0, 0);
#endif
PBL_SHELL_SUBCMD_ADD(sub_notif, ancs, sub_notif_ancs, "Stand-in iOS notifications", NULL, 0, 0);
PBL_SHELL_SUBCMD_ADD(sub_notif, list, NULL, "List stored notifications and their status",
                     prv_cmd_list, 0, 0);

#endif
