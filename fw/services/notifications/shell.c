/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_SHELL

#include <pbl/shell/shell.h>

#ifdef CONFIG_TOUCH
#include <pbl/drivers/rtc.h>
#include <pbl/services/notifications/notifications.h>
#include <pbl/services/timeline/item.h>

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

PBL_SHELL_SUBCMD_SET_CREATE(sub_notif);
PBL_SHELL_CMD_REGISTER(notif, sub_notif, "Notifications", NULL);

#ifdef CONFIG_TOUCH
PBL_SHELL_SUBCMD_ADD(sub_notif, test, NULL, "Add a long scrollable test notification", prv_cmd_test,
                     0, 0);
#endif

#endif
