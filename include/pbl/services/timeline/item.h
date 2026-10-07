/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "attribute.h"
#include "layout_layer.h"

#include "pbl/kernel/compiler.h"
#include "pbl/util/uuid.h"

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * @defgroup services_timeline_item Timeline items
 * @ingroup services_timeline
 * @brief Pins, notifications and reminders.
 *
 * A TimelineItem is a CommonTimelineItemHeader plus an attribute list and an action group. Items
 * built by the functions here keep all attributes, actions and strings in one buffer,
 * @ref TimelineItem::allocated_buffer, allocated on the calling task's heap together with the item.
 *
 * Serialized (BlobDB, notification storage) an item is a SerializedTimelineItemHeader followed by
 * the payload described in @ref services_timeline_attributes_actions.
 *
 * Building a pin with an action that opens its app:
 *
 * @code{.c}
 * AttributeList attr_list = {0};
 * attribute_list_add_cstring(&attr_list, AttributeIdTitle, "Workout");
 * attribute_list_add_cstring(&attr_list, AttributeIdBody, "45 min run");
 * attribute_list_add_resource_id(&attr_list, AttributeIdIconPin,
 *                                TIMELINE_RESOURCE_TIMELINE_SPORTS);
 *
 * AttributeList open_attrs = {0};
 * attribute_list_add_cstring(&open_attrs, AttributeIdTitle, "Open");
 * attribute_list_add_uint32(&open_attrs, AttributeIdLaunchCode, 42);
 * TimelineItemActionGroup action_group = {
 *   .num_actions = 1,
 *   .actions = (TimelineItemAction[]){
 *     {.id = 0, .type = TimelineItemActionTypeOpenWatchApp, .attr_list = open_attrs},
 *   },
 * };
 *
 * TimelineItem *pin = timeline_item_create_with_attributes(
 *     start, 45, TimelineItemTypePin, LayoutIdGeneric, &attr_list, &action_group);
 * attribute_list_destroy_list(&attr_list);
 * attribute_list_destroy_list(&open_attrs);
 * if (pin) {
 *   pin->header.parent_id = app_uuid;
 *   pin->header.from_watch = true;
 *   timeline_add(pin);
 *   timeline_item_destroy(pin);
 * }
 * @endcode
 * @{
 */

/** @brief Maximum length of a pin title, in bytes. */
#define MAX_PIN_TITLE_LENGTH (50)

/** @brief Action id meaning "no action". */
#define TIMELINE_INVALID_ACTION_ID (0xFF)

/** @brief Id of a timeline item. */
typedef Uuid TimelineItemId;

/** @brief Bits of CommonTimelineItemHeader::status. */
typedef enum {
  /** The item has been read. */
  TimelineItemStatusRead = 1 << 0,
  /** The item has been deleted. */
  TimelineItemStatusDeleted = 1 << 1,
  /** The item has been actioned on. */
  TimelineItemStatusActioned = 1 << 2,
  /** The reminder has been shown. */
  TimelineItemStatusReminded = 1 << 3,
  /** The item has been dismissed. */
  TimelineItemStatusDismissed = 1 << 4,
  /** Mask of the bits not covered above; set bits mark a corrupt stored header. */
  TimelineItemStatusUnused = ~((1 << 5) - 1)
} TimelineItemStatus;

/** @brief Bits of CommonTimelineItemHeader::flags. */
typedef enum {
  /** The item is visible. */
  TimelineItemFlagVisible = 1 << 0,
  /** The item is in floating time (local wall-clock time). */
  TimelineItemFlagFloating = 1 << 1,
  /** The item is an all-day event. */
  TimelineItemFlagAllDay = 1 << 2,
  /** The item was added by the watch. */
  TimelineItemFlagFromWatch = 1 << 3,
  /** The notification came from ANCS. */
  TimelineItemFlagFromANCS = 1 << 4,
  /** The item stays in Timeline Peek for its whole duration. */
  TimelineItemFlagPersistent = 1 << 5,
  /** Mask of the bits not covered above. */
  TimelineItemFlagUnused = ~((1 << 6) - 1)
} TimelineItemFlag;

/** @brief Types of actions, as used on the wire. */
typedef enum {
  /** Unknown action. */
  TimelineItemActionTypeUnknown = 0x00,
  /** ANCS negative action, formerly ANCS dismiss. */
  TimelineItemActionTypeAncsNegative = 0x01,
  /** Generic action executed by the phone. */
  TimelineItemActionTypeGeneric = 0x02,
  /** Reply with canned responses, voice or emoji, sent by the phone. */
  TimelineItemActionTypeResponse = 0x03,
  /** Dismiss; handled locally for items that the phone does not track. */
  TimelineItemActionTypeDismiss = 0x04,
  /** HTTP request made by the phone. */
  TimelineItemActionTypeHttp = 0x05,
  /** Snooze a reminder. */
  TimelineItemActionTypeSnooze = 0x06,
  /** Open the parent watch app with the AttributeIdLaunchCode launch code. */
  TimelineItemActionTypeOpenWatchApp = 0x07,
  /** Placeholder with no effect. */
  TimelineItemActionTypeEmpty = 0x08,
  /** Remove the pin; done locally for pins added by the watch. */
  TimelineItemActionTypeRemove = 0x09,
  /** Open the parent pin. */
  TimelineItemActionTypeOpenPin = 0x0A,
  /** ANCS positive action. */
  TimelineItemActionTypeAncsPositive = 0x0B,
  /** Call back the caller of an ANCS notification. */
  TimelineItemActionTypeAncsDial = 0x0C,
  /** Reply to an ANCS notification through the phone app. */
  TimelineItemActionTypeAncsResponse = 0x0D,
  /** Response to a health insight. */
  TimelineItemActionTypeInsightResponse = 0x0E,
  /** ANCS delete action. */
  TimelineItemActionTypeAncsDelete = 0x0F,
  /** Mark the item as completed, executed by the phone. */
  TimelineItemActionTypeComplete = 0x10,
  /** Postpone the item, executed by the phone. */
  TimelineItemActionTypePostpone = 0x11,
  /** Remove the item, executed by the phone. */
  TimelineItemActionTypeRemoteRemove = 0x12,
  /** Generic ANCS action executed by the phone app. */
  TimelineItemActionTypeAncsGeneric = 0x13,
  /** Stop sharing heart rate with BLE HRM clients. */
  TimelineItemActionTypeBLEHRMStopSharing = 0x14,
} TimelineItemActionType;

/** @brief Identifiers of icons in the resource pack. */
typedef enum {
  /** Cross mark. */
  TimelineItemIconIdCrossmark = 1,
  /** Check mark. */
  TimelineItemIconIdCheckmark,
  /** Sent mail. */
  TimelineItemIconIdSentMail,
  /** Sent message. */
  TimelineItemIconIdSentMessage,
  /** Phone with a check mark. */
  TimelineItemIconIdPhoneCheckmark
} TimelineItemIconId;

/** @brief Types of timeline items. */
typedef enum {
  /** Unknown type. */
  TimelineItemTypeUnknown = 0,
  /** Notification. */
  TimelineItemTypeNotification,
  /** Timeline pin. */
  TimelineItemTypePin,
  /** Reminder; its parent is a pin. */
  TimelineItemTypeReminder,
  /** First invalid value. */
  TimelineItemTypeOutOfRange
} TimelineItemType;

/** @brief Action of a timeline item. */
typedef struct {
  /** Action id, unique within the item, sent to the phone when invoked. */
  uint8_t id;
  /** Action type. */
  TimelineItemActionType type;
  /** Attributes of the action, e.g. its title. */
  AttributeList attr_list;
} TimelineItemAction;

/** @brief Actions of a timeline item. */
typedef struct {
  /** Number of actions. */
  uint8_t num_actions;
  /** Actions. */
  TimelineItemAction *actions;
} TimelineItemActionGroup;

/** @brief Header common to all timeline items, as stored. */
typedef struct PBL_PACKED {
  /**
   * Unique identifier of the item. Controlled by the watch; needed to answer the phone for actions
   * actuated on the watch.
   */
  TimelineItemId id;
  /** Parent or ANCS reference. */
  union {
    /**
     * ANCS UID of the item, if it was received from an iOS device but not through the Pebble iOS
     * application.
     */
    uint32_t ancs_uid;
    /** Identifier of the parent of this item: an app or data source, or the pin of a reminder. */
    TimelineItemId parent_id;
  };
  /**
   * Time at which the item occurs, in seconds since the epoch. UTC, except for all-day and
   * floating items in serialized headers, which use local time.
   */
  int32_t timestamp;
  /**
   * Duration in minutes. A pin stays in the NOW section of the timeline until timestamp plus
   * duration.
   */
  uint16_t duration;
  /** Item type. */
  TimelineItemType type : 8;
  /**
   * One-way flags set by the data source or mobile application describing how the item interacts
   * with the user. Once written to flash, the item has to be rewritten entirely to revert them.
   */
  union {
    /** Individual flags. */
    struct {
      /** The item is visible. */
      uint8_t visible : 1;
      /** The item is in floating time (local wall-clock time). */
      uint8_t is_floating : 1;
      /** The item is an all-day event; its timestamp should be at midnight. */
      uint8_t all_day : 1;
      /** The item was added by the watch and should not be flushed. */
      uint8_t from_watch : 1;
      /** The notification was received through ANCS (iOS). */
      uint8_t ancs_notif : 1;
    };
    /** All flags, see TimelineItemFlag. */
    uint8_t flags;
  };
  /**
   * One-way status bits. Once written to flash, the item has to be rewritten entirely to revert
   * them.
   */
  union {
    /** Individual status bits. */
    struct {
      /** The item has been read (notifications only). */
      uint8_t read : 1;
      /** The item has been deleted. */
      uint8_t deleted : 1;
      /** The item has been actioned on. */
      uint8_t actioned : 1;
      /** The reminder has been shown. */
      uint8_t reminded : 1;
      /** The item has been dismissed. */
      uint8_t dismissed : 1;
      /** The item stays in Timeline Peek for its whole duration. */
      uint8_t persistent : 1;
    };
    /** All status bits, see TimelineItemStatus. */
    uint8_t status;
  };
  /** Layout used to render the item, which determines how the attributes are shown. */
  LayoutId layout : 8;
} CommonTimelineItemHeader;

/**
 * @brief A reminder, notification or pin.
 *
 * The type in the header decides whether the item appears in the timeline or in the
 * notifications app.
 */
typedef struct {
  /** Common header. */
  CommonTimelineItemHeader header;
  /** Attributes describing the item. */
  AttributeList attr_list;
  /** Actions of the item. */
  TimelineItemActionGroup action_group;
  /** Buffer holding the attributes, actions and strings, or NULL. */
  uint8_t *allocated_buffer;
} TimelineItem;

/** @brief Header of a serialized item, followed by @ref payload_length bytes of payload. */
typedef struct PBL_PACKED {
  /** Common header. */
  CommonTimelineItemHeader common;
  /** Length of the payload in bytes. */
  uint16_t payload_length;
  /** Number of attributes, which determine how the item looks when rendered. */
  uint8_t num_attributes;
  /** Number of actions, offered when the item is rendered. */
  uint8_t num_actions;
} SerializedTimelineItemHeader;

/**
 * @brief Create an item from an attribute list and action group.
 *
 * The item gets a new random id. The attributes, actions and strings are deep copied into the
 * item's buffer, so the sources may be freed afterwards.
 *
 * @param timestamp Time of the item.
 * @param duration Duration in minutes.
 * @param type Item type.
 * @param layout Layout of the item.
 * @param attr_list Attributes to copy.
 * @param action_group Actions to copy, may be NULL.
 * @return Item allocated on the calling task's heap, or NULL if the copy failed. Free it with
 *         timeline_item_destroy().
 */
TimelineItem *timeline_item_create_with_attributes(time_t timestamp, uint16_t duration,
                                                   TimelineItemType type, LayoutId layout,
                                                   AttributeList *attr_list,
                                                   TimelineItemActionGroup *action_group);

/**
 * @brief Create an empty item with room for attributes, actions and strings.
 *
 * The header is zeroed. Fill the attributes and actions in place and append their strings at
 * @p string_buffer.
 *
 * @param num_attributes Number of non-action attributes.
 * @param num_actions Number of actions.
 * @param attributes_per_action Attribute count of each action, in action order.
 * @param required_size_for_strings Total size of all attribute strings.
 * @param[out] string_buffer Start of the string space, if not NULL.
 * @return Item allocated on the calling task's heap, or NULL if out of memory.
 */
TimelineItem *timeline_item_create(int num_attributes, int num_actions,
                                   uint8_t attributes_per_action[],
                                   size_t required_size_for_strings, uint8_t **string_buffer);

/**
 * @brief Allocate an item's buffer to deserialize a payload into.
 *
 * @param[out] item Item whose attribute list, action group and buffer are set up.
 * @param num_attributes Number of attributes.
 * @param num_actions Number of actions.
 * @param data Serialized payload.
 * @param size Size of @p data in bytes.
 * @param[out] string_alloc_size Size of the string space.
 * @param[out] string_buffer Start of the string space.
 * @return false on a parse error or if out of memory.
 */
bool timeline_item_create_from_serial_data(TimelineItem *item, uint8_t num_attributes,
                                           uint8_t num_actions, const uint8_t *data, size_t size,
                                           size_t *string_alloc_size, uint8_t **string_buffer);

/**
 * @brief Deep copy an item.
 *
 * @param src Item to copy, may be NULL.
 * @return Copy allocated on the calling task's heap, or NULL.
 */
TimelineItem *timeline_item_copy(TimelineItem *src);

/**
 * @brief Deserialize an item.
 *
 * @param[out] item_out Item; its buffer is allocated on the calling task's heap.
 * @param header Serialized header.
 * @param payload Serialized payload of @c header->payload_length bytes.
 * @return true on success.
 */
bool timeline_item_deserialize_item(TimelineItem *item_out,
                                    const SerializedTimelineItemHeader *header,
                                    const uint8_t *payload);

/**
 * @brief Fill a serialized header from an item.
 *
 * @param item Item.
 * @param[out] header Serialized header, including the payload length.
 */
void timeline_item_serialize_header(TimelineItem *item, SerializedTimelineItemHeader *header);

/**
 * @brief Fill an item's header and counts from a serialized header.
 *
 * The timestamp of all-day and floating items is converted from local time to UTC.
 *
 * @param[out] item Item.
 * @param header Serialized header.
 */
void timeline_item_deserialize_header(TimelineItem *item,
                                      const SerializedTimelineItemHeader *header);

/**
 * @brief Get the UTC timestamp of a serialized header.
 *
 * All-day and floating items store local time, which is converted to UTC; other timestamps are
 * returned unchanged.
 *
 * @param hdr Header.
 * @return UTC timestamp.
 */
time_t timeline_item_get_tz_timestamp(CommonTimelineItemHeader *hdr);

/**
 * @brief Serialize the attributes and actions of an item.
 *
 * @param item Item.
 * @param[out] buffer Output buffer.
 * @param buffer_size Size of @p buffer in bytes.
 * @return Number of bytes written.
 */
size_t timeline_item_serialize_payload(TimelineItem *item, uint8_t *buffer, size_t buffer_size);

/**
 * @brief Get the serialized size of an item's attributes and actions.
 *
 * @param item Item.
 * @return Size in bytes.
 */
size_t timeline_item_get_serialized_payload_size(TimelineItem *item);

/**
 * @brief Deserialize attributes and actions into an item laid out for them.
 *
 * @param[in,out] item Item set up with timeline_item_create_from_serial_data().
 * @param string_buffer String space.
 * @param string_buffer_size Size of @p string_buffer in bytes.
 * @param payload Serialized payload.
 * @param payload_size Size of @p payload in bytes.
 * @return true on success.
 */
bool timeline_item_deserialize_payload(TimelineItem *item, char *string_buffer,
                                       size_t string_buffer_size, const uint8_t *payload,
                                       size_t payload_size);

/**
 * @brief Free an item allocated by the functions in this group, and its buffer.
 *
 * @param item Item, may be NULL.
 */
void timeline_item_destroy(TimelineItem *item);

/**
 * @brief Free an item's buffer and set it to NULL.
 *
 * Pointers to attributes and actions become invalid. Suitable for items that are not themselves
 * dynamically allocated.
 *
 * @param item Item.
 */
void timeline_item_free_allocated_buffer(TimelineItem *item);

/**
 * @brief Check that a serialized item is well formed and has the attributes its layout requires.
 *
 * @param val Serialized header and payload.
 * @param val_len Length of @p val in bytes.
 * @return true if valid.
 */
bool timeline_item_verify_layout_serialized(const uint8_t *val, int val_len);

/**
 * @brief Find an action by id.
 *
 * @param item Item.
 * @param action_id Action id.
 * @return Action, or NULL if not found or the item id is invalid.
 */
const TimelineItemAction *timeline_item_find_action_with_id(const TimelineItem *item,
                                                            uint8_t action_id);

/**
 * @brief Find the first action of a type.
 *
 * @param item Item.
 * @param type Action type.
 * @return Action, or NULL if not found or the item id is invalid.
 */
TimelineItemAction *timeline_item_find_action_by_type(const TimelineItem *item,
                                                      TimelineItemActionType type);

/**
 * @brief Check whether an action dismisses its item.
 *
 * @param action Action.
 * @return true for dismiss and ANCS negative actions.
 */
bool timeline_item_action_is_dismiss(const TimelineItemAction *action);

/**
 * @brief Check whether an action is performed through ANCS.
 *
 * @param action Action.
 * @return true for ANCS negative, positive, delete and dial actions.
 */
bool timeline_item_action_is_ancs(const TimelineItemAction *action);

/**
 * @brief Find the first dismiss action of an item.
 *
 * @param item Item.
 * @return Dismiss or ANCS negative action, or NULL if not found or the item id is invalid.
 */
TimelineItemAction *timeline_item_find_dismiss_action(const TimelineItem *item);

/**
 * @brief Find the reply action of an item.
 *
 * @param item Item.
 * @return Response or ANCS response action, or NULL if not found or the item id is invalid.
 */
TimelineItemAction *timeline_item_find_reply_action(const TimelineItem *item);

/**
 * @brief Find the reply action of an action group.
 *
 * @param action_group Actions to search.
 * @return Response or ANCS response action, or NULL if not found.
 */
TimelineItemAction *timeline_item_action_group_find_reply_action(
    const TimelineItemActionGroup *action_group);

/**
 * @brief Check whether an item was received through ANCS (iOS).
 *
 * @param item Item.
 * @return true if it came from ANCS.
 */
bool timeline_item_is_ancs_notif(const TimelineItem *item);

/** @} */
