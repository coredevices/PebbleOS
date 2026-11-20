/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "ancs.h"

#include "ancs_app_name_storage.h"

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/bluetooth/ancs.h>
#include <pbl/bluetooth/clients/ancs.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/services/evented_timer.h>
#include <pbl/services/notifications/ancs/ancs_notifications.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>
#include <pbl/util/units.h>

#include <kernel/event_loop.h>

PBL_LOG_MODULE_DECLARE(bt, CONFIG_BT_LOG_LEVEL);

// -----------------------------------------------------------------------------
// Static function prototypes

static void prv_get_notification_attributes(uint32_t uid);

static void prv_release_attributes(void);

PBL_T_STATIC void prv_check_ancs_alive(void);

static void prv_perform_action(uint32_t notification_uid, enum pbl_bt_ancs_action action);

static void prv_op_timeout_start(void);

static void prv_op_timeout_stop(void);

// -----------------------------------------------------------------------------
// Static variables
//
// All accesses to these variables should happen from the KernelMain task,
// therefore no concurrent accesses can happen and no lock is needed.

#define INVALID_NOTIFICATION_UID 0xFFFFFFFF

#define ANCS_RETRY_TIME_MS (5 * PBL_MSEC_PER_SEC)

// Bounds the notification queue so a post-reconnect ANCS burst from many
// chatty apps cannot grow it without limit. Action ops are not capped — they
// are user-initiated and rare.
#define ANCS_NOTIF_QUEUE_MAX_DEPTH 16

// Bound the wait for a Control Point response so a lost reply can't wedge the
// in-flight queue head forever (which blocks every later notification).
#define ANCS_OP_RESPONSE_TIMEOUT_SECONDS 10

//! The attributes asked for of a notification. Phones that know no action labels answer
//! without them.
static const struct pbl_bt_ancs_attr_req s_notif_attr_reqs[ANCS_NOTIF_ATTR_COUNT] = {
  [ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL] =
      {.id = PBL_BT_ANCS_NOTIF_ATTR_POSITIVE_ACTION_LABEL, .optional = true},
  [ANCS_NOTIF_ATTR_NEGATIVE_ACTION_LABEL] =
      {.id = PBL_BT_ANCS_NOTIF_ATTR_NEGATIVE_ACTION_LABEL, .optional = true},
  [ANCS_NOTIF_ATTR_APP_ID] = {.id = PBL_BT_ANCS_NOTIF_ATTR_APP_IDENTIFIER},
  [ANCS_NOTIF_ATTR_TITLE] = {.id = PBL_BT_ANCS_NOTIF_ATTR_TITLE, .max_len = ANCS_TITLE_MAX_LENGTH},
  [ANCS_NOTIF_ATTR_SUBTITLE] =
      {.id = PBL_BT_ANCS_NOTIF_ATTR_SUBTITLE, .max_len = ANCS_SUBTITLE_MAX_LENGTH},
  [ANCS_NOTIF_ATTR_MESSAGE] =
      {.id = PBL_BT_ANCS_NOTIF_ATTR_MESSAGE, .max_len = ANCS_MESSAGE_MAX_LENGTH},
  [ANCS_NOTIF_ATTR_DATE] = {.id = PBL_BT_ANCS_NOTIF_ATTR_DATE, .max_len = ANCS_DATE_LENGTH},
};

static const struct pbl_bt_ancs_attr_req s_app_attr_reqs[ANCS_APP_ATTR_COUNT] = {
  [ANCS_APP_ATTR_DISPLAY_NAME] = {.id = PBL_BT_ANCS_APP_ATTR_DISPLAY_NAME},
};

//! Room for the longest response: its header, and each attribute with its own.
#define RESPONSE_MAX_LENGTH                                                                   \
  (5 + ANCS_NOTIF_ATTR_COUNT * sizeof(struct pbl_bt_ancs_attr) + 2 * ANCS_ACTION_MAX_LENGTH + \
   ANCS_APP_ID_MAX_LENGTH + ANCS_TITLE_MAX_LENGTH + ANCS_SUBTITLE_MAX_LENGTH +                \
   ANCS_MESSAGE_MAX_LENGTH + ANCS_DATE_LENGTH)

PBL_BT_ANCS_CLIENT_DEFINE(s_client, RESPONSE_MAX_LENGTH);

// The client calls back on the host task; the session runs on KernelMain.

typedef enum {
  ClientEventDiscovered,
  ClientEventSubscribed,
  ClientEventNotification,
  ClientEventNotifAttrs,
  ClientEventAppAttrs,
  ClientEventActionPerformed,
} ClientEventType;

typedef struct {
  ClientEventType type;
  uint32_t session;
  int status;
  uint32_t uid;
  enum pbl_bt_ancs_source source;
  struct pbl_bt_ancs_notif notif;
  //! Copies of the attributes got, in data; null for the ones the response lacked.
  const struct pbl_bt_ancs_attr *attrs[ANCS_NOTIF_ATTR_COUNT];
  uint8_t data[];
} ClientEvent;

typedef enum {
  NotificationQueueOpGetAttributes = 0,
  NotificationQueueOpPerformAction,
} NotificationQueueOp;

typedef struct {
  ListNode list_node;
  NotificationQueueOp op;
  uint32_t uid;
  enum pbl_bt_ancs_action action_id; // Only valid if op == NotificationQueueOpPerformAction
  // Only valid if op == NotificationQueueOpGetAttributes:
  struct pbl_bt_ancs_notif notif;
  //! Pre-existing notification seen right after reconnecting, possibly missed while disconnected
  bool recovered;
} NotificationQueueNode;

typedef struct ANCSClient {
  ANCSClientState state;
  uint16_t conn_handle;
  //! Whether the service was found: its characteristics can be used.
  bool discovered;
  struct pbl_wallclock_work is_alive_work;
  struct pbl_wallclock_work is_alive_response_work;
  // Watchdog for the in-flight Control Point request.
  struct pbl_wallclock_work op_timeout_work;
  //! The attributes of the notification at the head of the queue, once got.
  ClientEvent *notif_attrs;
  NotificationQueueNode *queue;
  bool alive_check_pending;
  // Alive-check intervals with no NS traffic; reset by any NS notification.
  // Triggers a CCCD re-subscribe at the threshold to recover silent NS.
  uint8_t alive_checks_without_ns;
  // Consecutive non-Idle alive checks; forces recovery at the threshold.
  uint8_t consecutive_busy_alive_checks;
  // Consecutive alive checks whose Control Point write iOS rejected.
  uint8_t consecutive_rejected_alive_checks;
} ANCSClient;

static ANCSClient *s_ancs_client;

// -----------------------------------------------------------------------------
// State Machine

static bool prv_can_transition_state(ANCSClientState new_state) {
  if (s_ancs_client->state == new_state) {
    return true;
  }

  switch (s_ancs_client->state) {
    case ANCSClientStateIdle:
      return (new_state == ANCSClientStateRequestedNotification ||
              new_state == ANCSClientStateRetrying ||
              new_state == ANCSClientStatePerformingAction ||
              new_state == ANCSClientStateAliveCheck);
    case ANCSClientStateRequestedNotification:
      return (new_state == ANCSClientStateRequestedApp || new_state == ANCSClientStateRetrying ||
              new_state == ANCSClientStateIdle);
    case ANCSClientStatePerformingAction:
      return (new_state == ANCSClientStateIdle);
    case ANCSClientStateRequestedApp:
      return (new_state == ANCSClientStateIdle);
    case ANCSClientStateAliveCheck:
      return (new_state == ANCSClientStateIdle);
    case ANCSClientStateRetrying:
      return (new_state == ANCSClientStateRequestedNotification ||
              new_state == ANCSClientStateIdle);
    default:
      pbl_panic();
  }
}

static void prv_set_state(ANCSClientState new_state) {
  PBL_ASSERTN(prv_can_transition_state(new_state));
  s_ancs_client->state = new_state;
}

#if UNITTEST
PBL_T_STATIC ANCSClientState prv_get_state(void) {
  return s_ancs_client->state;
}
#endif

// -----------------------------------------------------------------------------
// Notification Queue Logic

static void prv_do_notif_queue_operation(void) {
  if (s_ancs_client->queue->op == NotificationQueueOpGetAttributes) {
    prv_get_notification_attributes(s_ancs_client->queue->uid);
  } else if (s_ancs_client->queue->op == NotificationQueueOpPerformAction) {
    prv_perform_action(s_ancs_client->queue->uid, s_ancs_client->queue->action_id);
  }
}

static bool prv_notif_queue_comparator(ListNode *found_node, void *data) {
  NotificationQueueNode *queue_node = (NotificationQueueNode *)found_node;
  NotificationQueueNode *key_node = (NotificationQueueNode *)data;
  return (queue_node->uid == key_node->uid) && (queue_node->op == key_node->op);
}

static NotificationQueueNode *prv_notif_queue_find(NotificationQueueNode *node) {
  return (NotificationQueueNode *)list_find((ListNode *)s_ancs_client->queue,
                                            prv_notif_queue_comparator, (void *)node);
}

static void prv_notif_queue_reset(void) {
  ListNode *head = (ListNode *)s_ancs_client->queue;
  ListNode *cur;
  while (head) {
    cur = head;
    list_remove(cur, &head, nullptr);
    pbl_free(cur);
  }
  s_ancs_client->queue = nullptr;
}

static void prv_notif_queue_push_common(NotificationQueueNode *node) {
  if (prv_notif_queue_find(node)) {
    // already in the queue
    PBL_LOG_WRN("ANCS item already in Queue");
    pbl_free(node);
    return;
  }

  if (s_ancs_client->state == ANCSClientStateIdle) {
    s_ancs_client->queue =
        (NotificationQueueNode *)list_prepend((ListNode *)s_ancs_client->queue, (ListNode *)node);
    prv_do_notif_queue_operation();
  } else {
    list_append((ListNode *)s_ancs_client->queue, (ListNode *)node);
  }
}

static void prv_notif_queue_push_action(uint32_t uid, enum pbl_bt_ancs_action action_id) {
  NotificationQueueNode *node = pbl_malloc(sizeof(NotificationQueueNode));
  if (!node) {
    PBL_LOG_WRN("ANCS action alloc failed, dropping (uid=%" PRIu32 ")", uid);
    return;
  }
  *node = (NotificationQueueNode){
    .op = NotificationQueueOpPerformAction,
    .uid = uid,
    .action_id = action_id
  };

  prv_notif_queue_push_common(node);
}

// Evict the oldest pending attribute request to make room for a newer one.
// Never the in-flight head or a user action. Returns true if one was freed.
static bool prv_evict_oldest_attr_request(void) {
  const NotificationQueueNode *in_flight =
      (s_ancs_client->state != ANCSClientStateIdle) ? s_ancs_client->queue : nullptr;
  NotificationQueueNode *node = s_ancs_client->queue;
  while (node) {
    NotificationQueueNode *next = (NotificationQueueNode *)list_get_next((ListNode *)node);
    if (node != in_flight && node->op == NotificationQueueOpGetAttributes) {
      PBL_LOG_WRN("ANCS queue full, evicting oldest notif (uid=%" PRIu32 ")", node->uid);
      list_remove((ListNode *)node, (ListNode **)&s_ancs_client->queue, nullptr);
      pbl_free(node);
      return true;
    }
    node = next;
  }
  return false;
}

static void prv_notif_queue_push_attr_request(const struct pbl_bt_ancs_notif *notif,
                                              bool recovered) {
  const uint32_t uid = notif->uid;

  if (list_count((ListNode *)s_ancs_client->queue) >= ANCS_NOTIF_QUEUE_MAX_DEPTH) {
    // Evict a stale entry rather than drop the fresh one.
    if (!prv_evict_oldest_attr_request()) {
      PBL_LOG_WRN("ANCS queue full, dropping notif (uid=%" PRIu32 ")", uid);
      return;
    }
  }

  NotificationQueueNode *node = pbl_malloc(sizeof(NotificationQueueNode));
  if (!node) {
    PBL_LOG_WRN("ANCS attr-request alloc failed, dropping (uid=%" PRIu32 ")", uid);
    return;
  }
  *node = (NotificationQueueNode){
    .op = NotificationQueueOpGetAttributes,
    .uid = uid,
    .notif = *notif,
    .recovered = recovered,
  };

  prv_notif_queue_push_common(node);
}

static void prv_notif_queue_pop(void) {
  NotificationQueueNode *temp = s_ancs_client->queue;
  if (temp) {
    list_remove((ListNode *)s_ancs_client->queue, (ListNode **)&s_ancs_client->queue, nullptr);
    pbl_free(temp);
  }
}

static void prv_notif_queue_next(void) {
  if (s_ancs_client->alive_check_pending) {
    prv_check_ancs_alive();
    return;
  }

  if (s_ancs_client->queue == nullptr) {
    // empty
    return;
  }

  prv_do_notif_queue_operation();
}

#if UNITTEST
PBL_T_STATIC uint32_t prv_get_queue_depth(void) {
  return list_count((ListNode *)s_ancs_client->queue);
}

PBL_T_STATIC bool prv_queue_contains_uid(uint32_t uid) {
  NotificationQueueNode key = {
    .op = NotificationQueueOpGetAttributes,
    .uid = uid,
  };
  return prv_notif_queue_find(&key) != nullptr;
}
#endif

// -----------------------------------------------------------------------------
// Reset & Error Handling

static void prv_reset_and_idle(void) {
  prv_op_timeout_stop();
  pbl_bt_ancs_client_cancel(&s_client);
  prv_set_state(ANCSClientStateIdle);
  prv_release_attributes();
  // Back to Idle: not wedged.
  s_ancs_client->consecutive_busy_alive_checks = 0;
}

static void prv_reset_and_retry(void *unused) {
  if (s_ancs_client == nullptr) {
    return;
  }

  prv_release_attributes();
  prv_notif_queue_next();
}

static void prv_reset_and_next(void) {
  prv_reset_and_idle();
  prv_notif_queue_pop();
  prv_notif_queue_next();
}

static void prv_reset_and_flush(void) {
  prv_reset_and_idle();
  prv_notif_queue_reset();
}

static void prv_reset_due_to_parse_error(void) {
  prv_reset_and_next();
}

static void prv_reset_due_to_bt_error(void) {
  prv_reset_and_flush();
}

// -----------------------------------------------------------------------------
// In-flight operation watchdog
//
// Drops the queue head if its Control Point response never arrives.

static void prv_op_timeout_launcher_task_cb(void *unused) {
  if (!s_ancs_client) {
    return;
  }
  prv_op_timeout_stop();
  PBL_LOG_WRN("ANCS op timed out (state=%d, uid=%" PRIu32 "); dropping stuck head",
              s_ancs_client->state, s_ancs_client->queue ? s_ancs_client->queue->uid : 0);
  // Drop the wedged head so the queue keeps draining.
  prv_reset_and_next();
}

static void prv_op_timeout_fn(struct pbl_work *work) {
  // Hop to KernelMain; ANCS state is single-threaded there.
  launcher_task_add_callback(prv_op_timeout_launcher_task_cb, nullptr);
}

static void prv_op_timeout_stop(void) {
  if (s_ancs_client && pbl_wallclock_work_is_running(&s_ancs_client->op_timeout_work)) {
    pbl_wallclock_work_stop(&s_ancs_client->op_timeout_work);
  }
}

static void prv_op_timeout_start(void) {
  if (!s_ancs_client) {
    return;
  }
  prv_op_timeout_stop();
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_ancs_client->op_timeout_work, 1,
                           ANCS_OP_RESPONSE_TIMEOUT_SECONDS);
}

// -----------------------------------------------------------------------------
// Is Alive Logic

#define ANCS_IS_ALIVE_NEXT_CHECK_TIME_MINUTES    15 // Check every 15 minutes for faster recovery
#define ANCS_IS_ALIVE_RESPONSE_WAIT_TIME_SECONDS 5  // 5 seconds

// Force a flush + resubscribe if still busy after this many alive checks
// (a wedge the op watchdog somehow missed).
#define ANCS_MAX_BUSY_ALIVE_CHECKS 2

// Force a CCCD refresh after this many consecutive alive-check intervals
// without any NS traffic. 6 h is short enough to recover the same day for a
// typical user but long enough that DND / overnight don't trigger spurious
// refreshes -- iOS handles redundant CCCD writes silently. Derived from the
// alive-check interval so future tuning of one doesn't desync the other.
#define ANCS_NO_NS_TIMEOUT_HOURS 6
#define ANCS_ALIVE_CHECKS_WITHOUT_NS_BEFORE_RESUBSCRIBE \
  ((ANCS_NO_NS_TIMEOUT_HOURS * 60) / ANCS_IS_ALIVE_NEXT_CHECK_TIME_MINUTES)

static void prv_is_ancs_alive_fn(struct pbl_work *work);
static void prv_is_ancs_alive_response_timeout_fn(struct pbl_work *work);

// iOS rejects Control Point writes (ATT error, typically Write Not Permitted)
// when it no longer allows this watch to read its notifications. Nothing on
// the watch side can recover that, so tell the user instead of looping.
#define ANCS_REJECTED_ALIVE_CHECKS_BEFORE_WARNING 2

// Persists across connections so reconnects don't repeat the warning.
static bool s_cp_rejected_warning_shown;

static void prv_handle_alive_check_rejected(int error) {
  if (++s_ancs_client->consecutive_rejected_alive_checks <
      ANCS_REJECTED_ALIVE_CHECKS_BEFORE_WARNING) {
    return;
  }
  if (s_cp_rejected_warning_shown) {
    return;
  }
  PBL_LOG_WRN("ANCS Control Point rejected %u times (error=%d); warning user",
              s_ancs_client->consecutive_rejected_alive_checks, error);
  s_cp_rejected_warning_shown = true;
  ancs_notifications_handle_access_denied();
}

static void prv_ancs_is_alive_schedule_next_check(void) {
  pbl_wallclock_work_stop(&s_ancs_client->is_alive_response_work);
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_ancs_client->is_alive_work, 60,
                           ANCS_IS_ALIVE_NEXT_CHECK_TIME_MINUTES);
}

static void prv_ancs_is_alive_start_response_wait_timer(void) {
  pbl_wallclock_work_stop(&s_ancs_client->is_alive_work);
  pbl_wallclock_work_start(&pbl_sys_workq_high, &s_ancs_client->is_alive_response_work, 1,
                           ANCS_IS_ALIVE_RESPONSE_WAIT_TIME_SECONDS);
}

static bool prv_ancs_is_alive_timer_running(void) {
  return pbl_wallclock_work_is_running(&s_ancs_client->is_alive_work) ||
         pbl_wallclock_work_is_running(&s_ancs_client->is_alive_response_work);
}

static void prv_ancs_is_alive_stop_timer(void) {
  if (prv_ancs_is_alive_timer_running()) {
    pbl_wallclock_work_stop(&s_ancs_client->is_alive_work);
    pbl_wallclock_work_stop(&s_ancs_client->is_alive_response_work);
  }
}

static void prv_ancs_is_alive_start_tracking(void) {
  if (prv_ancs_is_alive_timer_running()) {
    prv_ancs_is_alive_stop_timer();
  } else {
    // Not scheduled, so analytics stopwatch would have been stopped
  }
  prv_ancs_is_alive_schedule_next_check();
}

static void prv_resubscribe_to_ancs(void) {
  if (!s_ancs_client) {
    return;
  }

  if (!s_ancs_client->discovered) {
    PBL_LOG_WRN("Cannot resubscribe to ANCS: service not found");
    return;
  }

  PBL_LOG_INFO("Re-subscribing to ANCS characteristics");

  // iOS may have silently dropped its subscription state (e.g. during a PPoGATT reset): write
  // the CCCDs again, off first.
  for (int source = PBL_BT_ANCS_SOURCE_DATA; source >= PBL_BT_ANCS_SOURCE_NOTIFICATION; --source) {
    (void)pbl_bt_ancs_client_subscribe(&s_client, source, false);

    const int rc = pbl_bt_ancs_client_subscribe(&s_client, source, true);
    if (rc != 0) {
      PBL_LOG_ERR("Failed to resubscribe to ANCS source %d: %d", source, rc);
    }
  }
}

static void prv_is_ancs_alive_response_timeout_launcher_task_cb(void *data) {
  if (!s_ancs_client) {
    return;
  }

  prv_reset_due_to_bt_error();

  // Stop the wait for response timer
  prv_ancs_is_alive_stop_timer();

  // Try to recover by re-subscribing to ANCS characteristics
  // This handles the case where iOS has dropped the GATT subscription
  // without the watch knowing about it (common during PPoGATT resets)
  prv_resubscribe_to_ancs();

  // Always re-arm the alive-check timer here, even though a successful
  // resubscribe will also re-arm it from prv_handle_subscribed(). Recovery is
  // not guaranteed: characteristics may be invalidated, the CCCD write may
  // fail synchronously, or iOS may never reply with a CCCD response. Without
  // this re-arm those paths leave the timer permanently stopped and the watch
  // stays connected but silent until reboot. The call is idempotent --
  // prv_ancs_is_alive_start_tracking() cancels and reschedules.
  prv_ancs_is_alive_schedule_next_check();
}

static void prv_is_ancs_alive_response_timeout_fn(struct pbl_work *work) {
  PBL_LOG_DBG("ANCS isn't alive");

  launcher_task_add_callback(prv_is_ancs_alive_response_timeout_launcher_task_cb, nullptr);
}

static void prv_ancs_is_alive(void) {
  PBL_LOG_DBG("ANCS is alive!");

  s_ancs_client->consecutive_rejected_alive_checks = 0;
  s_cp_rejected_warning_shown = false;

  // Restart analytics tracking (if it stopped) and the 'is alive' timer
  prv_ancs_is_alive_start_tracking();
}

PBL_T_STATIC void prv_check_ancs_alive(void) {
  // Stop the next check timer
  prv_ancs_is_alive_stop_timer();

  if (s_ancs_client) {
    s_ancs_client->alive_check_pending = false;

    // If iOS has been silent on NS for too long, the CCCD subscription is
    // likely wedged on iOS' side even though CP responses look fine. Force a
    // re-subscribe to nudge iOS into resuming pushes, and skip this round's CP
    // probe -- the next 15 min alive check will verify recovery.
    if (++s_ancs_client->alive_checks_without_ns >=
        ANCS_ALIVE_CHECKS_WITHOUT_NS_BEFORE_RESUBSCRIBE) {
      PBL_LOG_WRN("ANCS NS silent for %u alive checks; forcing CCCD resubscribe",
                  s_ancs_client->alive_checks_without_ns);
      s_ancs_client->alive_checks_without_ns = 0;
      prv_resubscribe_to_ancs();
      // prv_handle_subscribed() will re-arm on success; schedule a backup so
      // we still tick if the CCCD response never lands.
      prv_ancs_is_alive_schedule_next_check();
      return;
    }

    prv_set_state(ANCSClientStateAliveCheck);
    //! Asks for a notification that does not exist: ANCS answers that the parameter is invalid.
    (void)pbl_bt_ancs_client_get_notif_attrs(&s_client, INVALID_NOTIFICATION_UID, nullptr, 0);
    prv_ancs_is_alive_start_response_wait_timer();
  }
}

PBL_T_STATIC void prv_is_ancs_alive_launcher_task_cb(void *data) {
  if (!s_ancs_client) {
    return;
  }
  if (s_ancs_client->state == ANCSClientStateIdle) {
    s_ancs_client->consecutive_busy_alive_checks = 0;
    prv_check_ancs_alive();
  } else if (++s_ancs_client->consecutive_busy_alive_checks >= ANCS_MAX_BUSY_ALIVE_CHECKS) {
    // Wedged past the op watchdog: force recovery so we don't go silent.
    PBL_LOG_ERR("ANCS busy for %u alive checks; forcing flush + resubscribe",
                s_ancs_client->consecutive_busy_alive_checks);
    s_ancs_client->consecutive_busy_alive_checks = 0;
    prv_reset_due_to_bt_error();
    prv_resubscribe_to_ancs();
    prv_ancs_is_alive_schedule_next_check();
  } else {
    // Legitimately processing; defer this round's probe.
    s_ancs_client->alive_check_pending = true;
  }
}

static void prv_is_ancs_alive_fn(struct pbl_work *work) {
  launcher_task_add_callback(prv_is_ancs_alive_launcher_task_cb, nullptr);
}

// -----------------------------------------------------------------------------
//! iOS re-sends the whole notification center as pre-existing right after (re)subscribing. Those
//! are only handed over as recovered (missed while disconnected) for a bit after reconnection.
//! Without a previous session to compare against, they are dropped.
#define POST_CONNECTION_IGNORE_TICKS (10 * PBL_TICK_HZ)
static uint64_t s_connected_ticks;
static bool s_connected_ticks_valid;
static bool s_session_active = false;
static bool s_had_session = false;

static void prv_start_post_connection_ignore_window(void) {
  s_connected_ticks = pbl_uptime_ticks();
  s_connected_ticks_valid = true;
}

static bool prv_just_connected(void) {
  return s_connected_ticks_valid &&
         (pbl_uptime_ticks() - s_connected_ticks) < POST_CONNECTION_IGNORE_TICKS;
}

// -----------------------------------------------------------------------------
// The attributes of the notification being handled

static void prv_release_attributes(void) {
  pbl_free(s_ancs_client->notif_attrs);
  s_ancs_client->notif_attrs = nullptr;
}

static void prv_put_ancs_message(const struct pbl_bt_ancs_attr *const *app_attrs) {
  ancs_notifications_handle_message(&s_ancs_client->queue->notif, s_ancs_client->queue->recovered,
                                    s_ancs_client->notif_attrs->attrs, app_attrs);
}

//! Whether the service answered that a request made no sense, e.g. for a notification that is
//! gone, rather than that something is wrong with the link.
static bool prv_is_invalid_param(int status) {
  return status == PBL_BT_ANCS_ERR_INVALID_PARAMETER;
}

static bool prv_is_parse_error(int status) {
  return status == -EBADMSG || status == -ENOBUFS;
}

// -----------------------------------------------------------------------------
// Get App Attributes request

static void prv_handle_app_attrs(const ClientEvent *event) {
  if (s_ancs_client->state != ANCSClientStateRequestedApp) {
    return;
  }

  if (prv_is_invalid_param(event->status)) {
    prv_reset_and_next();
    return;
  }

  const struct pbl_bt_ancs_attr *app_attrs[ANCS_APP_ATTR_COUNT] = {};

  if (event->status == 0 && event->attrs[ANCS_APP_ATTR_DISPLAY_NAME] != nullptr) {
    app_attrs[ANCS_APP_ATTR_DISPLAY_NAME] = event->attrs[ANCS_APP_ATTR_DISPLAY_NAME];

    // cache the app name
    const struct pbl_bt_ancs_attr *app_id =
        s_ancs_client->notif_attrs->attrs[ANCS_NOTIF_ATTR_APP_ID];
    ancs_app_name_storage_store(app_id, app_attrs[ANCS_APP_ATTR_DISPLAY_NAME]);
  } else if (event->status == 0 || prv_is_parse_error(event->status)) {
    PBL_LOG_WRN("Error parsing app attributes");
  } else {
    PBL_LOG_ERR("Control point error response: %d", event->status);
    prv_reset_due_to_bt_error();
    return;
  }

  // The notification is put even without the name of its app.
  prv_put_ancs_message(app_attrs);
  prv_reset_and_next();
}

static void prv_get_app_attributes(const struct pbl_bt_ancs_attr *app_id) {
  prv_set_state(ANCSClientStateRequestedApp);
  prv_op_timeout_start();

  const int rc = pbl_bt_ancs_client_get_app_attrs(&s_client, app_id->value, app_id->len,
                                                  s_app_attr_reqs, PBL_ARRAY_SIZE(s_app_attr_reqs));
  if (rc != 0) {
    PBL_LOG_WRN("Failed to fetch app attributes for notification");
    const struct pbl_bt_ancs_attr *empty_attrs[ANCS_APP_ATTR_COUNT] = {};
    // we failed to fetch the app, but we got a notification
    prv_put_ancs_message(empty_attrs);
    prv_reset_and_next();
  }
}

// -----------------------------------------------------------------------------
// Get Notification Attributes request

static void prv_get_notification_attributes(uint32_t uid) {
  bool retrying = (s_ancs_client->state == ANCSClientStateRetrying);
  prv_set_state(ANCSClientStateRequestedNotification);
  // Arm before the request so completion/failure can cancel it.
  prv_op_timeout_start();

  const int rc = pbl_bt_ancs_client_get_notif_attrs(&s_client, uid, s_notif_attr_reqs,
                                                    PBL_ARRAY_SIZE(s_notif_attr_reqs));
  if (rc != 0) {
    PBL_LOG_DBG("Control point write error: %d", rc);
    if (retrying) {
      prv_reset_and_flush();
    } else {
      // Waiting on the retry timer, not a response.
      prv_op_timeout_stop();
      prv_set_state(ANCSClientStateRetrying);
      evented_timer_register(ANCS_RETRY_TIME_MS, false, prv_reset_and_retry, nullptr);
    }
  }
}

//! @return Whether the event is kept, for its attributes.
static bool prv_handle_notif_attrs(ClientEvent *event) {
  if (s_ancs_client->state == ANCSClientStateAliveCheck) {
    if (prv_is_invalid_param(event->status)) {
      // We got a response so cancel the response wait timer and setup another check.
      prv_ancs_is_alive();
      prv_reset_and_next();
    } else if (event->status != 0) {
      PBL_LOG_ERR("Control point error response: %d", event->status);
      prv_handle_alive_check_rejected(event->status);
      prv_reset_due_to_bt_error();
    }
    return false;
  }

  if (s_ancs_client->state != ANCSClientStateRequestedNotification ||
      s_ancs_client->queue == nullptr || s_ancs_client->queue->uid != event->uid) {
    return false;
  }

  if (prv_is_invalid_param(event->status)) {
    // We asked for a non-existent notification, go to the next one
    prv_reset_and_next();
    return false;
  }

  if (prv_is_parse_error(event->status)) {
    PBL_LOG_ERR("Error parsing attributes: %d", event->status);
    prv_reset_due_to_parse_error();
    return false;
  }

  if (event->status != 0) {
    PBL_LOG_ERR("Control point error response: %d", event->status);
    prv_reset_due_to_bt_error();
    return false;
  }

  s_ancs_client->notif_attrs = event;

  const struct pbl_bt_ancs_attr *app_id = event->attrs[ANCS_NOTIF_ATTR_APP_ID];
  const struct pbl_bt_ancs_attr *app_name = ancs_app_name_storage_get(app_id);
  if (app_name) {
    prv_put_ancs_message(&app_name);
    prv_reset_and_next();
  } else {
    prv_get_app_attributes(app_id);
  }

  return true;
}

// -----------------------------------------------------------------------------
// Subscriptions and the Notification Source

static void prv_put_ancs_disconnected_event(void) {
  if (s_session_active) {
    s_session_active = false;
    s_had_session = true;
    ancs_notifications_handle_session_lost();
  }

  PebbleEvent event = {
    .type = PEBBLE_ANCS_DISCONNECTED_EVENT,
  };
  event_put(&event);
}

static void prv_handle_subscribed(enum pbl_bt_ancs_source source, int error) {
  if (error == 0) {
    PBL_LOG_INFO("ANCS subscribed: %u", source);

    if (source == PBL_BT_ANCS_SOURCE_DATA) {
      prv_ancs_is_alive_start_tracking();
      prv_start_post_connection_ignore_window();
      s_session_active = true;
    }
  } else {
    PBL_LOG_ERR("Failed to subscribe source: %u (error=%d)", source, error);
  }
}

static void prv_invalidate_all_references(void) {
  s_ancs_client->discovered = false;

  prv_reset_and_flush();
  prv_put_ancs_disconnected_event();
}

static void prv_handle_service_discovered(void) {
  PBL_LOG_DBG("In ANCS service discovery CB");

  // Pause while re-subscribing, it will be resumed when re-subscribed:
  prv_ancs_is_alive_stop_timer();

  s_ancs_client->discovered = true;

  // Subscribe to the Data Source, then to the Notification Source. Reject the service
  // if subscribing fails instead of asserting.
  for (int source = PBL_BT_ANCS_SOURCE_DATA; source >= PBL_BT_ANCS_SOURCE_NOTIFICATION; --source) {
    const int rc = pbl_bt_ancs_client_subscribe(&s_client, source, true);
    if (rc != 0) {
      PBL_LOG_WRN("Failed to subscribe ANCS source %d (err=%d), ignoring service", source, rc);
      prv_invalidate_all_references();
      return;
    }
  }
}

static void prv_handle_notification(const struct pbl_bt_ancs_notif *notif) {
  // Any NS traffic (add/modify/remove) proves the subscription is still
  // delivering, so the silent-NS resubscribe watchdog can stand down.
  s_ancs_client->alive_checks_without_ns = 0;

  PBL_LOG_VERBOSE("NSNotification: ");
  PBL_LOG_VERBOSE("> EventID: %d", notif->event);
  PBL_LOG_VERBOSE("> EventFlags: <%d>", notif->flags);
  PBL_LOG_VERBOSE("> CategoryID: <%d>", notif->category);
  PBL_LOG_VERBOSE("> CategoryCount: <%d>", notif->category_count);
  PBL_LOG_VERBOSE("> NotificationUID: <%" PRIu32 ">", notif->uid);

  // Handle the EventID
  switch (notif->event) {
    case PBL_BT_ANCS_EVENT_ADDED:
      // In iOS 8.2 several apps (especially mail.app) seem to be setting the pre-existing flag
      // when they shouldn't. This appeared to be fixed in iOS 9 beta 1.
      // By skipping the pre-existing check we will re-receive all the notifications
      // we got in the past 2 hours. To get past this ignore notifications for the first couple
      // seconds after connecting
      if (prv_just_connected() && (notif->flags & PBL_BT_ANCS_FLAG_PRE_EXISTING)) {
        if (s_had_session) {
          PBL_LOG_DBG("PreExisting ANCS notification after reconnecting");
          prv_notif_queue_push_attr_request(notif, true);
        } else {
          PBL_LOG_DBG("Ignoring notification because we just connected and PreExisting");
        }
      } else {
        PBL_LOG_DBG("Added ANCS notification!");
        prv_notif_queue_push_attr_request(notif, false);
      }

      break;
    case PBL_BT_ANCS_EVENT_MODIFIED:
      PBL_LOG_DBG("Modified ANCS notification!");
      prv_notif_queue_push_attr_request(notif, false);
      break;
    case PBL_BT_ANCS_EVENT_REMOVED:
      PBL_LOG_DBG("Removed ANCS notification");
      ancs_notifications_handle_notification_removed(notif);
      break;
  }
}

// -------------------------------------------------------------------------------------------------
// Performing ANCS Notification Actions

static void prv_handle_action_performed(const ClientEvent *event) {
  if (s_ancs_client->state != ANCSClientStatePerformingAction) {
    return;
  }

  if (event->status == 0 || prv_is_invalid_param(event->status)) {
    prv_reset_and_next();
  } else {
    PBL_LOG_ERR("Control point error response: %d", event->status);
    prv_reset_due_to_bt_error();
  }
}

static void prv_perform_action(uint32_t notification_uid, enum pbl_bt_ancs_action action) {
  prv_set_state(ANCSClientStatePerformingAction);

  PBL_LOG_DBG("Taking action <%u> upon UID: %" PRIu32, action, notification_uid);

  prv_op_timeout_start();
  if (pbl_bt_ancs_client_perform_action(&s_client, notification_uid, action) != 0) {
    prv_reset_and_next();
  }
}

typedef struct {
  uint32_t notification_uid;
  uint8_t action_id;
} PerformActionData;

static void prv_serialize_action(const PerformActionData *action) {
  if (!s_ancs_client) {
    PBL_LOG_ERR("No ANCS client");
    return;
  }

  prv_notif_queue_push_action(action->notification_uid, action->action_id);
}

static void prv_serialize_action_launcher_task_cb(void *data) {
  prv_serialize_action(data);
  pbl_free(data);
}

void ancs_perform_action(uint32_t notification_uid, uint8_t action_id) {
  bool is_kernel_main = (pebble_task_get_current() == PebbleTask_KernelMain);
  // Avoid heap allocation when directly calling prv_serialize_action:
  PerformActionData action;
  PerformActionData *action_ptr =
      is_kernel_main ? &action : pbl_malloc_check(sizeof(PerformActionData));
  *action_ptr = (const PerformActionData){
    .notification_uid = notification_uid,
    .action_id = action_id,
  };
  if (is_kernel_main) {
    prv_serialize_action(action_ptr);
  } else {
    launcher_task_add_callback(prv_serialize_action_launcher_task_cb, action_ptr);
  }
}

// -------------------------------------------------------------------------------------------------
// Lifecycle

//! At most this many events wait for KernelMain; notifications past it are dropped.
#define CLIENT_EVENTS_MAX 32

//! Tells the events of a session that ended from those of the current one.
static uint32_t s_session;
static atomic_uint s_num_client_events;

static void prv_client_event_kernel_main_cb(void *data) {
  ClientEvent *event = data;
  bool kept = false;

  atomic_fetch_sub(&s_num_client_events, 1);

  if (s_ancs_client && event->session == s_session) {
    switch (event->type) {
      case ClientEventDiscovered:
        // The service moved or went, if it was found before.
        if (s_ancs_client->discovered) {
          prv_invalidate_all_references();
        }
        if (event->status == 0) {
          prv_handle_service_discovered();
        } else {
          PBL_LOG_DBG("No ANCS (%d)", event->status);
        }
        break;
      case ClientEventSubscribed:
        prv_handle_subscribed(event->source, event->status);
        break;
      case ClientEventNotification:
        if (s_ancs_client->discovered) {
          prv_handle_notification(&event->notif);
        }
        break;
      case ClientEventNotifAttrs:
        kept = prv_handle_notif_attrs(event);
        break;
      case ClientEventAppAttrs:
        prv_handle_app_attrs(event);
        break;
      case ClientEventActionPerformed:
        prv_handle_action_performed(event);
        break;
    }
  }

  if (!kept) {
    pbl_free(event);
  }
}

//! Puts an event with copies of the attributes that came with it, if any.
static void prv_put_client_event(const ClientEvent *template,
                                 const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  if (atomic_fetch_add(&s_num_client_events, 1) >= CLIENT_EVENTS_MAX &&
      template->type == ClientEventNotification) {
    atomic_fetch_sub(&s_num_client_events, 1);
    PBL_LOG_WRN("Dropping ANCS notification");
    return;
  }

  size_t data_size = 0;
  for (uint8_t i = 0; attrs != nullptr && i < num_attrs; i++) {
    if (attrs[i] != nullptr) {
      data_size += sizeof(*attrs[i]) + attrs[i]->len;
    }
  }

  // On the host task: an event is lost rather than the watch reset when memory is short.
  ClientEvent *event = pbl_malloc(sizeof(*event) + data_size);
  if (event == nullptr) {
    atomic_fetch_sub(&s_num_client_events, 1);
    PBL_LOG_WRN("Dropping ANCS event %d: out of memory", template->type);
    return;
  }
  *event = *template;
  event->session = s_session;

  uint8_t *data = event->data;
  for (uint8_t i = 0; attrs != nullptr && i < num_attrs; i++) {
    if (attrs[i] != nullptr) {
      const size_t size = sizeof(*attrs[i]) + attrs[i]->len;
      memcpy(data, attrs[i], size);
      event->attrs[i] = (struct pbl_bt_ancs_attr *)data;
      data += size;
    }
  }

  launcher_task_add_callback(prv_client_event_kernel_main_cb, event);
}

static void prv_client_discovered(struct pbl_bt_ancs_client *client, int status) {
  prv_put_client_event(&(ClientEvent){.type = ClientEventDiscovered, .status = status}, nullptr, 0);
}

static void prv_client_subscribed(struct pbl_bt_ancs_client *client, enum pbl_bt_ancs_source source,
                                  bool enable, int status) {
  if (enable) {
    prv_put_client_event(
        &(ClientEvent){.type = ClientEventSubscribed, .source = source, .status = status}, nullptr,
        0);
  }
}

static void prv_client_notification(struct pbl_bt_ancs_client *client,
                                    const struct pbl_bt_ancs_notif *notif) {
  prv_put_client_event(&(ClientEvent){.type = ClientEventNotification, .notif = *notif}, nullptr,
                       0);
}

static void prv_client_notif_attrs(struct pbl_bt_ancs_client *client, int status, uint32_t uid,
                                   const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  prv_put_client_event(&(ClientEvent){.type = ClientEventNotifAttrs, .status = status, .uid = uid},
                       attrs, num_attrs);
}

static void prv_client_app_attrs(struct pbl_bt_ancs_client *client, int status,
                                 const struct pbl_bt_ancs_attr *const *attrs, uint8_t num_attrs) {
  prv_put_client_event(&(ClientEvent){.type = ClientEventAppAttrs, .status = status}, attrs,
                       num_attrs);
}

static void prv_client_action_performed(struct pbl_bt_ancs_client *client, int status,
                                        uint32_t uid) {
  prv_put_client_event(
      &(ClientEvent){.type = ClientEventActionPerformed, .status = status, .uid = uid}, nullptr, 0);
}

static const struct pbl_bt_ancs_client_cb s_client_cb = {
  .discovered = prv_client_discovered,
  .subscribed = prv_client_subscribed,
  .notification = prv_client_notification,
  .notif_attrs = prv_client_notif_attrs,
  .app_attrs = prv_client_app_attrs,
  .action_performed = prv_client_action_performed,
};

static void prv_client_start(void) {
  ++s_session;
  const int rc = pbl_bt_ancs_client_start(&s_client, s_ancs_client->conn_handle, &s_client_cb);
  if (rc != 0) {
    PBL_LOG_ERR("Could not look for ANCS (%d)", rc);
  }
}

void ancs_handle_services_changed(void) {
  if (!s_ancs_client) {
    return;
  }

  // The session goes on if the service is still where it was: subscribing again would have
  // the phone tell of every notification it has.
  const int rc = pbl_bt_ancs_client_rediscover(&s_client);
  if (rc != 0) {
    PBL_LOG_ERR("Could not look for ANCS again (%d)", rc);
  }
}

void ancs_create(uint16_t conn_handle) {
  PBL_ASSERTN(s_ancs_client == nullptr);
  s_ancs_client = (ANCSClient *)pbl_zalloc_check(sizeof(ANCSClient));
  s_ancs_client->conn_handle = conn_handle;
  pbl_wallclock_work_init(&s_ancs_client->is_alive_work, prv_is_ancs_alive_fn);
  pbl_wallclock_work_init(&s_ancs_client->is_alive_response_work,
                          prv_is_ancs_alive_response_timeout_fn);
  pbl_wallclock_work_init(&s_ancs_client->op_timeout_work, prv_op_timeout_fn);
  ancs_app_name_storage_init();
  prv_client_start();
}

void ancs_destroy(void) {
  if (!s_ancs_client) {
    return;
  }
  pbl_bt_ancs_client_stop(&s_client);
  ++s_session;
  prv_ancs_is_alive_stop_timer();
  prv_op_timeout_stop();

  ancs_app_name_storage_deinit();

  prv_reset_and_flush();
  pbl_free(s_ancs_client);
  s_ancs_client = nullptr;
  prv_put_ancs_disconnected_event();
}
