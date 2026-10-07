/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file accessory_notifications.c
//! Consumer of decrypted Apple AccessoryNotifications payloads.
//!
//! The BLE transport service (`accessory_transport_service.c`) decrypts each
//! forwarded notification and calls the consumer this file registers with it via
//! `accessory_transport_service_set_handler` (from `accessory_notifications_init`),
//! passing the transport featureID it arrived on. The payload is the companion
//! app's TLV wire format:
//!   [u8 msgType][…]
//!     msgType 0x01 present/update, 0x02 remove-one, 0x03 remove-all
//!   for 0x01: TLVs  tag(u8) len(u8) value:
//!     0x01 title, 0x02 subtitle, 0x03 body, 0x04 source, 0x05 identifier,
//!     0x06 alert(1 byte) — consumed but ignored; the watch's own DND/alert policy
//!       governs popup/vibe, like ANCS,
//!     0x07 action = [u8 flags|u8 action_id_len|action_id|u8 title_len|title]
//!       flags bit 0 (AN_ACTION_FLAG_TEXT_INPUT): the action collects a reply,
//!     0x08 source identifier = the source app's bundle id (UTF-8, at most
//!       SETTINGS_KEY_MAX_LEN bytes) — keys the app's iOSNotifPrefs, so Mute works
//!       like ANCS,
//!     0x09 replies = entries of [u8 len (>= 1)|len bytes of UTF-8], no terminators,
//!       the count implied by the TLV length: the canned replies for every text-input
//!       action, in order. Without it the watch offers its own. A malformed value is
//!       ignored as a whole.
//!
//! It builds a standard notification TimelineItem (with an action menu) and hands
//! it to `notifications_add_notification`, so forwarded notifications display like
//! ANCS ones. Selecting an action calls `accessory_notifications_invoke_action`,
//! which seals the reply back to the phone via the transport (AccessoryToHost).
//! The decrypt runs on the BT host task, so the work is marshaled to the system task.

#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "pbl/bluetooth/accessory_transport.h"
#include "pbl/drivers/rtc.h"
#include "pbl/logging/logging.h"
#include "pbl/services/blob_db/ios_notif_pref_db.h"
#include "pbl/services/notifications/accessory_notifications.h"
#include "pbl/services/notifications/ancs/ancs_filtering.h"
#include "pbl/services/notifications/notification_storage.h"
#include "pbl/services/notifications/notifications.h"
#include "pbl/services/settings/settings_raw_iter.h"
#include "pbl/services/system_task.h"
#include "pbl/services/timeline/attribute.h"
#include "pbl/services/timeline/item.h"
#include "pbl/services/timeline/layout_layer.h"
#include "pbl/services/timeline/timeline.h"
#include "pbl/util/uuid.h"

// The name-based UUID below hashes with mbed TLS directly. Like hpke.c, this relies
// on CONFIG_BT selecting MBEDTLS, so every firmware that builds this file has it.
#include "mbedtls/sha256.h"

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

// Wire message types.
#define AN_MSG_PRESENT    0x01u
#define AN_MSG_REMOVE_ONE 0x02u
#define AN_MSG_REMOVE_ALL 0x03u

// Present-message TLV tags.
#define AN_TAG_TITLE      0x01u
#define AN_TAG_SUBTITLE   0x02u
#define AN_TAG_BODY       0x03u
#define AN_TAG_SOURCE     0x04u
#define AN_TAG_IDENTIFIER 0x05u
#define AN_TAG_ALERT      0x06u
#define AN_TAG_ACTION     0x07u
#define AN_TAG_SOURCE_ID  0x08u
#define AN_TAG_REPLIES    0x09u

//! Action flag bits (first byte of an AN_TAG_ACTION value). Bit 0 marks a
//! text-input action (iOS AccessoryNotification.Action .textInput) — the watch
//! collects a reply and sends it back as the response's user text.
#define AN_ACTION_FLAG_TEXT_INPUT 0x01u

#define AN_MAX_ACTIONS 4
// 7 item strings (title/subtitle/body/source/source id/featureID/notificationID), up to
// two strings (id + title) per action, and one replies list shared by the text-input actions.
#define AN_MAX_ALLOCS (7 + 2 * AN_MAX_ACTIONS + 1)

// A fixed namespace so these UUIDs share no space with any other name-based UUID
// the system might mint. Generated once, at random; only its constancy matters.
static const uint8_t s_uuid_namespace[16] = {
  0x9b, 0x2e, 0x6a, 0x1c, 0x4f, 0x83, 0x4d, 0x2a, 0xa7, 0x0f, 0x1d, 0x84, 0x30, 0x77, 0x5a, 0xe1,
};

//! Deterministic 16-byte Uuid from the iOS notification identifier, so remove-one
//! and updates map to the same watch notification. SHA-256 over namespace ||
//! identifier, truncated to 16 bytes with the RFC 9562 version (8, custom) and
//! variant bits set — all 128 bits carry entropy, so distinct identifiers don't
//! collide. It never leaves the watch (the phone doesn't compute it).
static void prv_uuid_from_identifier(const uint8_t *id, size_t len, Uuid *out) {
  uint8_t bytes[16] = {0};
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  if (mbedtls_sha256_starts(&ctx, 0) == 0 &&
      mbedtls_sha256_update(&ctx, s_uuid_namespace, sizeof(s_uuid_namespace)) == 0 &&
      mbedtls_sha256_update(&ctx, id, len) == 0 && mbedtls_sha256_finish(&ctx, digest) == 0) {
    memcpy(bytes, digest, 16);
    bytes[6] = (uint8_t)((bytes[6] & 0x0F) | 0x80); // RFC 9562 version 8
    bytes[8] = (uint8_t)((bytes[8] & 0x3F) | 0x80); // RFC 9562 variant
  } else {
    // Leaves bytes all-zero, which would collide every id to one UUID — warn so it
    // is diagnosable rather than a silent mis-map.
    PBL_LOG_WRN("AN: SHA-256 failed deriving notification UUID");
  }
  mbedtls_sha256_free(&ctx);
  *out = UuidMakeFromBEBytes(bytes);
}

// Forwarded notification ids, so remove-all skips ANCS ones. System task only;
// lazily allocated (nothing resident until used); bounded, oldest evicted.
#define AN_MAX_TRACKED 64
static Uuid *s_tracked;
static uint8_t s_tracked_count;

static void prv_track_add(const Uuid *u) {
  if (!s_tracked) {
    s_tracked = kernel_malloc(sizeof(Uuid) * AN_MAX_TRACKED);
    if (!s_tracked) {
      return; // best-effort tracking; skip if the allocation fails
    }
  }
  for (uint8_t i = 0; i < s_tracked_count; i++) {
    if (uuid_equal(&s_tracked[i], u)) {
      return;
    }
  }
  if (s_tracked_count < AN_MAX_TRACKED) {
    s_tracked[s_tracked_count++] = *u;
  } else {
    memmove(&s_tracked[0], &s_tracked[1], (AN_MAX_TRACKED - 1) * sizeof(Uuid));
    s_tracked[AN_MAX_TRACKED - 1] = *u;
  }
}

static void prv_track_remove(const Uuid *u) {
  for (uint8_t i = 0; i < s_tracked_count; i++) {
    if (uuid_equal(&s_tracked[i], u)) {
      memmove(&s_tracked[i], &s_tracked[i + 1], (s_tracked_count - i - 1) * sizeof(Uuid));
      s_tracked_count--;
      return;
    }
  }
}

// Drop one forwarded notification from storage, the UI, and the tracked set. Must run
// on the system task (it touches s_tracked).
static void prv_remove(Uuid *id) {
  notification_storage_remove(id);
  notifications_handle_notification_removed(id);
  prv_track_remove(id);
}

typedef struct {
  size_t data_len;
  uint8_t feature_id[ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN]; // not NUL-terminated
  uint8_t feature_id_len;
  uint8_t bytes[];
} AccessoryNotifPayload;

//! Allocate `size` bytes, tracked for later free.
static void *prv_alloc(size_t size, char **allocs, int *n) {
  if (*n >= AN_MAX_ALLOCS) {
    // Out of alloc slots: a field is dropped, which can make actions silently fail.
    PBL_LOG_WRN("AN: alloc-slot cap (%d) hit; dropping a field", AN_MAX_ALLOCS);
    return NULL;
  }
  char *p = kernel_malloc(size);
  if (p) {
    allocs[(*n)++] = p;
  }
  return p;
}

//! Allocate a NUL-terminated copy of `len` bytes, tracked for later free.
static char *prv_dup(const uint8_t *src, size_t len, char **allocs, int *n) {
  char *s = prv_alloc(len + 1, allocs, n);
  if (!s) {
    return NULL;
  }
  memcpy(s, src, len);
  s[len] = '\0';
  return s;
}

//! Parse an AN_TAG_REPLIES value — entries of u8 len | len bytes — into a struct pbl_string_list
//! (allocated via `allocs`) for AttributeIdCannedResponses. NULL, with a warning, when
//! the value is malformed, so it is ignored as a whole; NULL for an empty value or on OOM.
static struct pbl_string_list *prv_parse_replies(const uint8_t *val, uint8_t vlen, char **allocs,
                                                 int *num_allocs) {
  // Validate every entry before building, so a bad one drops the whole value. A NUL
  // inside an entry would split it in two in the list, shifting every later reply.
  size_t off = 0;
  while (off < vlen) {
    const uint8_t len = val[off++];
    if (len == 0 || off + len > vlen || memchr(&val[off], '\0', len)) {
      PBL_LOG_WRN("AN: ignoring malformed replies (%u B)", (unsigned)vlen);
      return NULL;
    }
    off += len;
  }
  if (vlen == 0) {
    return NULL;
  }
  // The entries plus one NUL each are exactly vlen bytes; the extra byte is one
  // pbl_string_list_add_string holds back for the first string and never uses. A u8 TLV
  // length keeps the list under MAX_LENGTH_CANNED_RESPONSES (512), past which storage
  // would clip it mid-reply.
  const size_t list_size = sizeof(struct pbl_string_list) + vlen + 1;
  struct pbl_string_list *list = prv_alloc(list_size, allocs, num_allocs);
  if (!list) {
    return NULL;
  }
  list->serialized_byte_length = 0;
  off = 0;
  while (off < vlen) {
    const uint8_t len = val[off++];
    if (pbl_string_list_add_string(list, list_size, (const char *)&val[off], len) != len) {
      PBL_LOG_WRN("AN: replies did not fit their list; ignoring them");
      return NULL; // the list is still freed with the other allocs
    }
    off += len;
  }
  return list;
}

//! Parse one AN_TAG_ACTION value — u8 flags | u8 action_id_len | action_id |
//! u8 title_len | title — into `action_out`/`attrs_out` (strings duplicated via
//! `allocs`). Returns false on a malformed value or an allocation failure.
static bool prv_parse_action(const uint8_t *val, uint8_t vlen, uint8_t action_index,
                             TimelineItemAction *action_out, AttributeList *attrs_out,
                             char **allocs, int *num_allocs) {
  if (vlen < 3) {
    return false;
  }
  uint8_t flags = val[0];
  uint8_t action_id_len = val[1];
  if ((size_t)(2 + action_id_len + 1) > vlen) {
    return false;
  }
  uint8_t title_len = val[2 + action_id_len];
  if ((size_t)(2 + action_id_len + 1 + title_len) > vlen) {
    return false;
  }
  char *action_id = prv_dup(&val[2], action_id_len, allocs, num_allocs);
  char *title = prv_dup(&val[3 + action_id_len], title_len, allocs, num_allocs);
  if (!action_id || !title) {
    return false;
  }
  *attrs_out = (AttributeList){0};
  attribute_list_add_cstring(attrs_out, AttributeIdTitle, title);
  attribute_list_add_cstring(attrs_out, AttributeIdAccessoryActionId, action_id); // not displayed
  // A text-input action becomes an AccessoryResponse (gets the reply menu:
  // canned/emoji/voice); a plain action is an AccessoryGeneric, invoked directly.
  const bool text_input = (flags & AN_ACTION_FLAG_TEXT_INPUT) != 0;
  *action_out = (TimelineItemAction){
    .id = action_index,
    .type = text_input ? TimelineItemActionTypeAccessoryResponse
                       : TimelineItemActionTypeAccessoryGeneric,
    .attr_list = *attrs_out,
  };
  return true;
}

//! Heap ANCSAttribute holding `len` bytes of `val`, so the AN path can share
//! ancs_filtering_record_app with ANCS. NULL for an empty value or on OOM.
static ANCSAttribute *prv_ancs_attribute(const char *val, size_t len) {
  if (!val || len == 0 || len > UINT16_MAX) {
    return NULL;
  }
  ANCSAttribute *attr = kernel_malloc(sizeof(ANCSAttribute) + len);
  if (attr) {
    attr->id = 0;
    attr->length = (uint16_t)len;
    memcpy(attr->value, val, len);
  }
  return attr;
}

//! Record the source app in the iOS notification prefs (creating the record the Mute
//! action edits, named after the display name) and report whether it is muted now —
//! the ANCS path's ancs_filtering_record_app + ancs_filtering_is_muted.
static bool prv_record_app_and_check_muted(const char *app_id, const char *display_name,
                                           const char *title) {
  const size_t app_id_len = strlen(app_id);
  ANCSAttribute *app_id_attr = prv_ancs_attribute(app_id, app_id_len);
  if (!app_id_attr) {
    return false;
  }
  ANCSAttribute *display_name_attr =
      prv_ancs_attribute(display_name, display_name ? strlen(display_name) : 0);
  ANCSAttribute *title_attr = prv_ancs_attribute(title, title ? strlen(title) : 0);

  iOSNotifPrefs *prefs = ios_notif_pref_db_get_prefs((const uint8_t *)app_id, (int)app_id_len);
  ancs_filtering_record_app(&prefs, app_id_attr, display_name_attr, title_attr);
  const bool muted = ancs_filtering_is_muted(prefs);
  ios_notif_pref_db_free_prefs(prefs);

  kernel_free(title_attr);
  kernel_free(display_name_attr);
  kernel_free(app_id_attr);
  return muted;
}

static void prv_present(const uint8_t *d, size_t n, const char *feature_id, size_t feature_id_len) {
  AttributeList attr_list = {0};
  char *allocs[AN_MAX_ALLOCS];
  int num_allocs = 0;
  bool have_content = false;
  const uint8_t *ident = NULL;
  uint8_t ident_len = 0;
  const uint8_t *source_id = NULL;
  uint8_t source_id_len = 0;
  const uint8_t *replies = NULL;
  uint8_t replies_len = 0;
  // Keep the first of each content tag so duplicates can't starve the reply-context allocs.
  uint16_t seen_tags = 0;

  // Forwarded notification actions become entries in the item's action menu, plus one
  // watch-local Dismiss appended below (hence AN_MAX_ACTIONS + 1).
  TimelineItemAction actions[AN_MAX_ACTIONS + 1];
  AttributeList action_attrs[AN_MAX_ACTIONS + 1];
  uint8_t num_actions = 0;

  size_t off = 1; // skip msgType
  while (off + 2 <= n) {
    uint8_t tag = d[off++];
    uint8_t vlen = d[off++];
    if (off + vlen > n) {
      break;
    }
    const uint8_t *val = &d[off];
    off += vlen;

    AttributeId attr_id;
    switch (tag) {
      case AN_TAG_TITLE:
        attr_id = AttributeIdTitle;
        break;
      case AN_TAG_SUBTITLE:
        attr_id = AttributeIdSubtitle;
        break;
      case AN_TAG_BODY:
        attr_id = AttributeIdBody;
        break;
      case AN_TAG_SOURCE:
        attr_id = AttributeIdAppName;
        break;
      case AN_TAG_IDENTIFIER:
        if (!ident) { // keep the first; ignore duplicates
          ident = val;
          ident_len = vlen;
        }
        continue;
      case AN_TAG_SOURCE_ID:
        // The prefs record is keyed by it, and a settings key can't be longer.
        if (vlen > SETTINGS_KEY_MAX_LEN) {
          PBL_LOG_WRN("AN: ignoring %u-byte source identifier", (unsigned)vlen);
        } else if (!source_id) { // keep the first; ignore duplicates
          source_id = val;
          source_id_len = vlen;
        }
        continue;
      case AN_TAG_REPLIES:
        if (!replies) { // keep the first; ignore duplicates
          replies = val;
          replies_len = vlen;
        }
        continue;
      case AN_TAG_ALERT:
        // Consumed but ignored: the watch's own policy (DND/prefs) governs the popup and
        // vibe, like ANCS — not iOS's per-notification alert byte.
        continue;
      case AN_TAG_ACTION:
        if (num_actions < AN_MAX_ACTIONS &&
            prv_parse_action(val, vlen, num_actions, &actions[num_actions],
                             &action_attrs[num_actions], allocs, &num_allocs)) {
          num_actions++;
        }
        continue;
      default:
        continue; // unknown tag
    }
    if (seen_tags & (uint16_t)(1u << tag)) {
      continue; // duplicate content tag; the first occurrence already claimed its slot
    }
    seen_tags |= (uint16_t)(1u << tag);
    char *s = prv_dup(val, vlen, allocs, &num_allocs);
    if (s) {
      attribute_list_add_cstring(&attr_list, attr_id, s);
      have_content = true;
    }
  }

  if (have_content && source_id && source_id_len > 0) {
    char *source_id_str = prv_dup(source_id, source_id_len, allocs, &num_allocs);
    if (source_id_str) {
      if (prv_record_app_and_check_muted(
              source_id_str, attribute_get_string(&attr_list, AttributeIdAppName, NULL),
              attribute_get_string(&attr_list, AttributeIdTitle, NULL))) {
        // Muted from the watch's own Mute action: drop it, updates included, like ANCS.
        PBL_LOG_DBG("AN: ignoring forwarded notification from <%s>: Muted", source_id_str);
        have_content = false;
      } else {
        attribute_list_add_cstring(&attr_list, AttributeIdiOSAppIdentifier, source_id_str);
      }
    }
  }

  if (have_content) {
    // Tag order isn't fixed, so the replies are applied once every action is parsed.
    struct pbl_string_list *reply_list = NULL;
    for (uint8_t i = 0; replies && i < num_actions; i++) {
      if (actions[i].type != TimelineItemActionTypeAccessoryResponse) {
        continue;
      }
      if (!reply_list) {
        reply_list = prv_parse_replies(replies, replies_len, allocs, &num_allocs);
        if (!reply_list) {
          break;
        }
      }
      attribute_list_add_string_list(&action_attrs[i], AttributeIdCannedResponses, reply_list);
      // Adding reallocates the list, so the action's copy of it must follow.
      actions[i].attr_list = action_attrs[i];
    }

    // Reply context stored on the item so an action can seal a reply later.
    if (feature_id_len > 0) {
      char *feature_id_str =
          prv_dup((const uint8_t *)feature_id, feature_id_len, allocs, &num_allocs);
      if (feature_id_str) {
        attribute_list_add_cstring(&attr_list, AttributeIdAccessoryFeatureId, feature_id_str);
      }
    }
    if (ident && ident_len > 0) {
      char *notification_id_str = prv_dup(ident, ident_len, allocs, &num_allocs);
      if (notification_id_str) {
        attribute_list_add_cstring(&attr_list, AttributeIdAccessoryNotificationId,
                                   notification_id_str);
      }
    }

    // Append a watch-local Dismiss so a forwarded notification (even action-less or
    // phone-action-only) can be dismissed on the watch, like ANCS. The notification
    // window special-cases TimelineItemActionTypeDismiss (root-level Dismiss/Dismiss All
    // and Select-to-dismiss); dismissing invokes the local-dismiss path. Distinct id
    // from the phone actions (0..num_actions-1). No title: prv_present runs on the system
    // task where i18n_get is unsafe, so the menu builder localizes the label on KernelMain
    // (timeline_actions_add_action_to_root_level).
    action_attrs[num_actions] = (AttributeList){0};
    actions[num_actions] = (TimelineItemAction){
      .id = num_actions,
      .type = TimelineItemActionTypeDismiss,
      .attr_list = action_attrs[num_actions],
    };
    num_actions++;

    TimelineItemActionGroup group = {0};
    group.num_actions = num_actions;
    group.actions = actions;
    TimelineItem *item = timeline_item_create_with_attributes(
        rtc_get_time(), 0, TimelineItemTypeNotification, LayoutIdNotification, &attr_list,
        num_actions ? &group : NULL);
    if (item) {
      bool is_update = false;
      uint8_t prev_status = 0;
      if (ident && ident_len > 0) {
        prv_uuid_from_identifier(ident, ident_len, &item->header.id);
        // Same identifier = update: read the live copy's status, then replace it (the
        // persistent check survives a reboot or eviction from the RAM tracked list).
        if (notification_storage_notification_exists(&item->header.id)) {
          is_update = true;
          notification_storage_get_status(&item->header.id, &prev_status);
        }
        notification_storage_remove(&item->header.id); // drop the old copy (no-op if none)
        prv_track_remove(&item->header.id);
      }
      // No identifier: keep the random UUID (and the type/layout)
      // timeline_item_create_with_attributes already set from its arguments.
      // Carry read/actioned/dismissed across an update so a replied or dismissed
      // notification isn't resurrected as fresh and actionable.
      item->header.read = (prev_status & TimelineItemStatusRead) != 0;
      item->header.actioned = (prev_status & TimelineItemStatusActioned) != 0;
      item->header.dismissed = (prev_status & TimelineItemStatusDismissed) != 0;
      if (is_update) {
        // Update: only replace storage, emit no event — matching ANCS (prv_handle_ancs_update).
        // A currently-shown copy refreshes on next reload, so the notification never
        // disappears and never re-alerts.
        notification_storage_store(item);
      } else {
        // New notification: always add so the watch's own policy (DND/prefs) governs the
        // popup/vibe, like ANCS. iOS's alert byte doesn't suppress display here.
        notifications_add_notification(item);
      }
      prv_track_add(&item->header.id);
      timeline_item_destroy(item);
      PBL_LOG_DBG("AN: presented forwarded notification (%u actions)", (unsigned)num_actions);
    }
  }

  attribute_list_destroy_list(&attr_list);
  for (uint8_t i = 0; i < num_actions; i++) {
    attribute_list_destroy_list(&action_attrs[i]);
  }
  for (int i = 0; i < num_allocs; i++) {
    kernel_free(allocs[i]);
  }
}

//! Remove-one (0x02): payload is the iOS identifier → same deterministic Uuid.
static void prv_remove_one(const uint8_t *id, size_t len) {
  if (len == 0) {
    return;
  }
  Uuid uuid;
  prv_uuid_from_identifier(id, len, &uuid);
  prv_remove(&uuid);
  PBL_LOG_DBG("AN: removed forwarded notification");
}

//! Remove-all (0x03): dismiss the forwarded notifications we still track. Best-effort:
//! the tracked set is RAM-only and capped, so pre-reboot/evicted ones aren't covered.
static void prv_remove_all(void) {
  for (uint8_t i = 0; i < s_tracked_count; i++) {
    notification_storage_remove(&s_tracked[i]);
    notifications_handle_notification_removed(&s_tracked[i]);
  }
  PBL_LOG_DBG("AN: removed all %u forwarded notifications", (unsigned)s_tracked_count);
  s_tracked_count = 0;
}

static void prv_process(void *ctx) {
  AccessoryNotifPayload *p = ctx;
  const uint8_t *d = p->bytes;
  const size_t n = p->data_len;
  if (n >= 1) {
    switch (d[0]) {
      case AN_MSG_PRESENT:
        prv_present(d, n, (const char *)p->feature_id, p->feature_id_len);
        break;
      case AN_MSG_REMOVE_ONE:
        prv_remove_one(&d[1], n - 1);
        break;
      case AN_MSG_REMOVE_ALL:
        prv_remove_all();
        break;
      default:
        PBL_LOG_ERR("AN: unknown message type 0x%02x", d[0]);
        break;
    }
  }
  kernel_free(p);
}

// Carries a reply's real send outcome from the transport completion (BT host task)
// to KernelMain, where the action result is posted.
typedef struct {
  Uuid id;
  bool ok;
} ANReplyResult;

// KernelMain: post the reply's true send outcome as the notification's action result.
// Must run here, not the system task: timeline_put_accessory_action_result → i18n_get is
// KernelMain-affine and unlocked, so posting from another task would race i18n's list.
static void prv_post_reply_result(void *ctx) {
  ANReplyResult *res = ctx;
  timeline_put_accessory_action_result(&res->id, res->ok);
  kernel_free(res);
}

// Transport send completion. Runs on the BT host task, so it can't post the result
// directly — marshal to KernelMain (i18n_get in the posted path is KernelMain-affine).
// Fires exactly once per accepted send; `res` is owned here and freed in
// prv_post_reply_result (launcher_task_add_callback always enqueues).
static void prv_reply_complete(void *ctx, bool ok) {
  ANReplyResult *res = ctx;
  res->ok = ok;
  launcher_task_add_callback(prv_post_reply_result, res);
}

// See accessory_notifications.h. Reply wire (to the transport):
//   u8 notification_id_len | notification_id | u8 action_id_len | action_id |
//   u16 text_len (LE) | text
bool accessory_notifications_invoke_action(const TimelineItem *item,
                                           const TimelineItemAction *action,
                                           const AttributeList *reply_attributes) {
  const char *feature_id =
      attribute_get_string(&item->attr_list, AttributeIdAccessoryFeatureId, NULL);
  const char *notification_id =
      attribute_get_string(&item->attr_list, AttributeIdAccessoryNotificationId, NULL);
  const char *action_id =
      attribute_get_string(&action->attr_list, AttributeIdAccessoryActionId, NULL);
  if (!feature_id || !notification_id || !action_id) {
    PBL_LOG_ERR("AN: action missing reply context");
    return false;
  }
  const char *text =
      reply_attributes ? attribute_get_string(reply_attributes, AttributeIdTitle, NULL) : NULL;
  const size_t notification_id_len = strlen(notification_id);
  const size_t action_id_len = strlen(action_id);
  const size_t text_len = text ? strlen(text) : 0;
  if (notification_id_len > 255 || action_id_len > 255 || text_len > 0xFFFF) {
    return false;
  }
  // Heap, not stack — this runs on the modal/UI task whose stack is small.
  const size_t plen = 1 + notification_id_len + 1 + action_id_len + 2 + text_len;
  uint8_t *payload = kernel_malloc(plen);
  if (!payload) {
    return false;
  }
  size_t o = 0;
  payload[o++] = (uint8_t)notification_id_len;
  memcpy(&payload[o], notification_id, notification_id_len);
  o += notification_id_len;
  payload[o++] = (uint8_t)action_id_len;
  memcpy(&payload[o], action_id, action_id_len);
  o += action_id_len;
  payload[o++] = (uint8_t)(text_len & 0xFF);
  payload[o++] = (uint8_t)((text_len >> 8) & 0xFF);
  if (text_len) {
    memcpy(&payload[o], text, text_len);
    o += text_len;
  }
  // The seal+notify runs later on the BT host task; carry the notification id so the
  // transport's completion can post the real Sent/Failed result for it.
  ANReplyResult *res = kernel_malloc(sizeof(*res));
  if (!res) {
    kernel_free(payload);
    return false;
  }
  res->id = item->header.id;
  // Returns true only if accepted for async delivery; the real outcome arrives via
  // prv_reply_complete. Don't mark actioned here — that follows the async success.
  const bool accepted = accessory_transport_service_send_response(
      feature_id, strlen(feature_id), payload, o, prv_reply_complete, res);
  kernel_free(payload); // send_response copied it
  if (!accepted) {
    PBL_LOG_ERR("AN: action reply not accepted for delivery");
    kernel_free(res); // completion won't fire, so free the ctx here
    return false;
  }
  PBL_LOG_DBG("AN: action reply accepted (%u text bytes); result follows", (unsigned)text_len);
  return true;
}

// The registered consumer: copies the payload + feature id and defers the parse to
// the system task (the caller is on the BT host task).
static void prv_notification_received(const uint8_t *data, size_t len, const char *feature_id,
                                      size_t feature_id_len) {
  // Reject rather than truncate: a clipped feature id would seal replies to the wrong
  // session, which the phone silently drops.
  if (len == 0 || len > ACCESSORY_TRANSPORT_MAX_PAYLOAD ||
      feature_id_len > ACCESSORY_TRANSPORT_MAX_FEATURE_ID_LEN) {
    PBL_LOG_WRN("AN: dropping oversized payload (%u B) / feature id (%u B)", (unsigned)len,
                (unsigned)feature_id_len);
    return;
  }
  AccessoryNotifPayload *p = kernel_malloc(sizeof(AccessoryNotifPayload) + len);
  if (!p) {
    return;
  }
  p->data_len = len;
  p->feature_id_len = (uint8_t)feature_id_len;
  memcpy(p->feature_id, feature_id, p->feature_id_len);
  memcpy(p->bytes, data, len);
  if (!system_task_add_callback(prv_process, p)) {
    kernel_free(p);
  }
}

void accessory_notifications_init(void) {
  accessory_transport_service_set_handler(prv_notification_received);
}
