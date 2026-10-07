/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "pbl/services/timeline/item.h"
#include "system/status_codes.h"
#include "pbl/util/iterator.h"

/**
 * @defgroup services_timeline Timeline
 * @ingroup services
 * @brief Pins, notifications and reminders, their attributes, actions and layouts.
 *
 * Every timeline entity is a TimelineItem (@ref services_timeline_item): a common header plus a
 * list of typed attributes (@ref services_timeline_attribute) and a group of actions. Pins are
 * kept in the Pins BlobDB, reminders in the Reminders BlobDB and notifications in notification
 * storage. Items are rendered by layouts (@ref services_timeline_layout_layer) chosen by the
 * header's LayoutId.
 *
 * This header manages pins: adding and removing them, invoking actions, and iterating over the
 * pins shown in the Timeline app. The iterator walks a time-ordered list of TimelineNode built by
 * timeline_init(); all-day events come first, then events by start time, and an event appears in
 * both the past and the future until it ends. Only pins within two days in the past and three days
 * in the future are visited.
 *
 * @code{.c}
 * TimelineNode *timeline = NULL;
 * Iterator iter;
 * TimelineIterState state = {0};
 *
 * timeline_init(&timeline);
 * if (timeline_iter_init(&iter, &state, &timeline, TimelineIterDirectionFuture, rtc_get_time()) ==
 *     S_SUCCESS) {
 *   do {
 *     const char *title = attribute_get_string(&state.pin.attr_list, AttributeIdTitle, "");
 *     ...
 *   } while (iter_next(&iter));
 * }
 * timeline_iter_deinit(&iter, &state, &timeline);
 * @endcode
 * @{
 */

/** @brief Opaque node of the ordered pin list. */
struct TimelineNode;
/** @brief Opaque node of the ordered pin list. */
typedef struct TimelineNode TimelineNode;

/** @brief Direction of a timeline iteration. */
typedef enum {
  /** Towards older pins. */
  TimelineIterDirectionPast,
  /** Towards newer pins. */
  TimelineIterDirectionFuture,
} TimelineIterDirection;

/** @brief State of a timeline iterator. */
typedef struct {
  /** Current node. */
  TimelineNode *node;
  /** Index of the current node in the list. */
  int index;
  /** Time the iteration started from. */
  time_t start_time;
  /** Direction of the iteration. */
  TimelineIterDirection direction;
  /** Current pin, read from the Pins database; its buffer is owned by the iterator. */
  TimelineItem pin;
  /** Whether all-day events are shown in this direction. */
  bool show_all_day_events;
  /** Midnight of the day of @ref start_time. */
  time_t midnight;
  /** Midnight of the day of the current pin. */
  time_t current_day;
} TimelineIterState;

/**
 * @brief Build the ordered pin list from the Pins database.
 *
 * Expired pins are deleted from the database on the way.
 *
 * @param[out] timeline Head of the new list.
 * @return S_SUCCESS or an error from the Pins database.
 */
status_t timeline_init(TimelineNode **timeline);

/**
 * @brief Add a pin created on the watch to the Pins database.
 *
 * The item is serialized, so destroy it with timeline_item_destroy() afterwards.
 *
 * @param item Pin to add.
 * @return true on success.
 */
bool timeline_add(TimelineItem *item);

/**
 * @brief Add a missed call pin.
 *
 * Gives @p pin a new id, the generic layout and the from-watch flag, and turns its dismiss action
 * into a remove action.
 *
 * @param pin Pin built from the missed call notification; it must have a dismiss action.
 * @param uid ANCS UID of the missed call notification.
 * @return true on success.
 */
bool timeline_add_missed_call_pin(TimelineItem *pin, uint32_t uid);

/**
 * @brief Remove a pin through BlobDB, which emits a BlobDB delete event.
 *
 * @param id Id of the pin.
 * @return true on success.
 */
bool timeline_remove(const Uuid *id);

/**
 * @brief Check whether a pin exists.
 *
 * @param id Id of the pin.
 * @return true if it exists in the Pins database.
 */
bool timeline_exists(Uuid *id);

/**
 * @brief Enable or disable bulk mode for ANCS actions.
 *
 * In bulk mode ANCS dismiss actions do not post a result dialog for each item, so dismissing many
 * items does not fill the event queue.
 *
 * @param enable true to enable.
 */
void timeline_enable_ancs_bulk_action_mode(bool enable);

/**
 * @brief Check whether bulk mode for ANCS actions is enabled.
 *
 * @return true if enabled.
 */
bool timeline_is_bulk_ancs_action_mode_enabled(void);

/**
 * @brief Invoke an action of a timeline item.
 *
 * Local actions (opening an app or the parent pin, removing a watch pin, dismissing a local
 * notification, ANCS actions) run on the watch; the others are sent to the phone, possibly over
 * Bluetooth. The outcome is reported as a PebbleSysNotificationActionResult.
 *
 * @param item Item the action belongs to.
 * @param action Action to invoke.
 * @param attributes Extra attributes sent with a remote action (e.g. a reply), may be NULL.
 */
void timeline_invoke_action(const TimelineItem *item, const TimelineItemAction *action,
                            const AttributeList *attributes);

//! Post a "Sent"/"Failed" notification action result (dialog) for `id`. Used by the
//! AccessoryNotifications transport's async send completion to report the real outcome
//! of a reply once the seal+notify has actually run.
void timeline_put_accessory_action_result(const Uuid *id, bool ok);

/**
 * @brief Get the direction in which a pin is shown.
 *
 * @param item Pin.
 * @param timeline Ordered pin list, used to place all-day events.
 * @param now Current time.
 * @return TimelineIterDirectionPast if the pin is in the past, TimelineIterDirectionFuture
 *         otherwise.
 */
TimelineIterDirection timeline_direction_for_item(TimelineItem *item, TimelineNode *timeline,
                                                  time_t now);

/**
 * @brief Compare two nodes.
 *
 * @param a First node, may be NULL.
 * @param b Second node, may be NULL.
 * @return true if both have the same id and timestamp, or both are NULL.
 */
bool timeline_nodes_equal(TimelineNode *a, TimelineNode *b);

/**
 * @brief Get the UUID of the originator of a timeline item.
 *
 * For pins and notifications this is the parent_id of the item, or of its parent pin if it has
 * one, i.e. the app UUID (pins) or source id (notifications). For reminders it is the parent_id of
 * the parent pin, the app UUID of the pin that created the reminder.
 *
 * @param item Item to inspect.
 * @param[out] id Originator id, UUID_INVALID on failure.
 * @return true on success, false for an invalid item type or a reminder without a parent pin.
 */
bool timeline_get_originator_id(const TimelineItem *item, Uuid *id);

/**
 * @brief Compare items in Timeline order, ignoring all-day events.
 *
 * Items shown in @p direction come first, then items are ordered by time.
 *
 * @param new_common Header the result refers to.
 * @param old_common Header compared against.
 * @param direction Timeline direction.
 * @return Negative if @p new_common goes before @p old_common, positive if after, 0 if equal.
 */
int timeline_item_time_comparator(CommonTimelineItemHeader *new_common,
                                  CommonTimelineItemHeader *old_common,
                                  TimelineIterDirection direction);

/**
 * @brief Check whether an item shows up in a Timeline direction, ignoring all-day events.
 *
 * @param header Header of the item.
 * @param direction Timeline direction.
 * @return true if the item would show up now.
 */
bool timeline_item_should_show(CommonTimelineItemHeader *header, TimelineIterDirection direction);

/**
 * @brief Start iterating over the pin list.
 *
 * On success @p iter_state holds the first pin. Advance with iter_next(), which reads the next pin
 * into @p iter_state; iter_prev() goes the other way.
 *
 * @param[out] iter Iterator to initialize.
 * @param[out] iter_state Iterator state.
 * @param timeline Ordered pin list from timeline_init().
 * @param direction Iteration direction.
 * @param timestamp Time to start from.
 * @return S_SUCCESS, S_NO_MORE_ITEMS if no pin is shown in @p direction, or a Pins database error.
 */
status_t timeline_iter_init(Iterator *iter, TimelineIterState *iter_state, TimelineNode **timeline,
                            TimelineIterDirection direction, time_t timestamp);

/**
 * @brief Copy an iterator into another one.
 *
 * The pin of @p dst_state is freed and left empty; refresh it with timeline_iter_refresh_pin().
 *
 * @param[out] dst_state Destination state.
 * @param src_state Source state.
 * @param[out] dst_iter Destination iterator.
 * @param src_iter Source iterator.
 */
void timeline_iter_copy_state(TimelineIterState *dst_state, TimelineIterState *src_state,
                              Iterator *dst_iter, Iterator *src_iter);

/**
 * @brief Free the pin list and the iterator's current pin.
 *
 * @param iter Iterator.
 * @param iter_state Iterator state.
 * @param[in,out] head Pin list; set to NULL.
 */
void timeline_iter_deinit(Iterator *iter, TimelineIterState *iter_state, TimelineNode **head);

/**
 * @brief Reload the current pin from the Pins database.
 *
 * Does not move the pin in the list if its timestamp changed. No-op if the pin no longer exists.
 *
 * @param iter_state Iterator state.
 */
void timeline_iter_refresh_pin(TimelineIterState *iter_state);

/**
 * @brief Remove a node from the pin list.
 *
 * @param[in,out] timeline Pin list.
 * @param node Node to remove and free.
 */
void timeline_iter_remove_node(TimelineNode **timeline, TimelineNode *node);

/**
 * @brief Remove the first node with an id from the pin list.
 *
 * Multi-day events have one node per day, so call repeatedly to remove all of them.
 *
 * @param[in,out] timeline Pin list.
 * @param key Pin id.
 * @return true if a node was found and removed.
 */
bool timeline_iter_remove_node_with_id(TimelineNode **timeline, Uuid *key);

/** @brief Data source of notifications, ed429c16-f674-4220-95da-454f303f15e2. */
#define UUID_NOTIFICATIONS_DATA_SOURCE \
  {0xed, 0x42, 0x9c, 0x16, 0xf6, 0x74, 0x42, 0x20, 0x95, 0xda, 0x45, 0x4f, 0x30, 0x3f, 0x15, 0xe2}

/** @brief Data source of calendar pins, 6c6c6fc2-1912-4d25-8396-3547d1dfac5b. */
#define UUID_CALENDAR_DATA_SOURCE \
  {0x6c, 0x6c, 0x6f, 0xc2, 0x19, 0x12, 0x4d, 0x25, 0x83, 0x96, 0x35, 0x47, 0xd1, 0xdf, 0xac, 0x5b}

/** @brief Data source of weather pins, 61b22bc8-1e29-460d-a236-3fe409a439ff. */
#define UUID_WEATHER_DATA_SOURCE \
  {0x61, 0xb2, 0x2b, 0xc8, 0x1e, 0x29, 0x46, 0xd, 0xa2, 0x36, 0x3f, 0xe4, 0x9, 0xa4, 0x39, 0xff}

/** @brief Data source of reminders, 42a07217-5491-4267-904a-d02a156752b6. */
#define UUID_REMINDERS_DATA_SOURCE \
  {0x42, 0xa0, 0x72, 0x17, 0x54, 0x91, 0x42, 0x67, 0x90, 0x4a, 0xd0, 0x2a, 0x15, 0x67, 0x52, 0xb6}

/** @brief Data source of alarm pins, 67a32d95-ef69-46d4-a0b9-854cc62f97f9. */
#define UUID_ALARMS_DATA_SOURCE \
  {0x67, 0xa3, 0x2d, 0x95, 0xef, 0x69, 0x46, 0xd4, 0xa0, 0xb9, 0x85, 0x4c, 0xc6, 0x2f, 0x97, 0xf9}

/** @brief Data source of health pins, 36d8c6ed-4c83-4fa1-a9e2-8f12dc941f8c. */
#define UUID_HEALTH_DATA_SOURCE \
  {0x36, 0xd8, 0xc6, 0xed, 0x4c, 0x83, 0x4f, 0xa1, 0xa9, 0xe2, 0x8f, 0x12, 0xdc, 0x94, 0x1f, 0x8c}

/** @brief Data source of workout pins, fef82c82-7176-4e22-88de-35a3fc18d43f. */
#define UUID_WORKOUT_DATA_SOURCE \
  {0xfe, 0xf8, 0x2c, 0x82, 0x71, 0x76, 0x4e, 0x22, 0x88, 0xde, 0x35, 0xa3, 0xfc, 0x18, 0xd4, 0x3f}

/** @brief Data source of the Send Text app, 0863fc6a-66c5-4f62-ab8a-82ed00a98b5d. */
#define UUID_SEND_TEXT_DATA_SOURCE \
  {0x08, 0x63, 0xfc, 0x6a, 0x66, 0xc5, 0x4f, 0x62, 0xab, 0x8a, 0x82, 0xed, 0x00, 0xa9, 0x8b, 0x5d}

/**
 * @brief Item id that lets the watch send an SMS to a phone number,
 * 0f71aaba-5814-4b5c-96e2-c9828c9734cb.
 */
#define UUID_SEND_SMS \
  {0x0f, 0x71, 0xaa, 0xba, 0x58, 0x14, 0x4b, 0x5c, 0x96, 0xe2, 0xc9, 0x82, 0x8c, 0x97, 0x34, 0xcb}

/** @brief Data source of intercom pins, 68010669-4b38-4751-ad04-067f1d8d2ab5. */
#define UUID_INTERCOM_DATA_SOURCE \
  {0x68, 0x01, 0x06, 0x69, 0x4b, 0x38, 0x47, 0x51, 0xad, 0x04, 0x06, 0x7f, 0x1d, 0x8d, 0x2a, 0xb5}

/**
 * @brief Get the name of a private (non-app) data source such as Weather or Calendar.
 *
 * @param parent_id Parent id of an item.
 * @return Untranslated name (an i18n key), or NULL if @p parent_id is not a private data source.
 */
const char *timeline_get_private_data_source(Uuid *parent_id);

/** @} */
