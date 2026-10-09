/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/util/string_list.h>

/**
 * @defgroup services_timeline_attribute Timeline attributes
 * @ingroup services_timeline
 * @brief Typed key/value attributes describing timeline items and actions.
 *
 * Each AttributeId has a fixed value type: a string, an 8- or 32-bit integer, a resource id, a
 * string list or a Uint32List. The type is noted on each id; the matching Attribute member holds
 * the value.
 *
 * On the wire (BlobDB, Pebble Protocol) an attribute is a SerializedAttributeHeader followed by
 * @c length bytes of value: strings without terminator, integers little endian, string lists as
 * their serialized bytes and Uint32Lists as the struct. When deserializing, the title and subtitle
 * are truncated to 64 bytes, the body and string lists to 512 bytes and the subtitle template
 * string to 150 bytes. Attributes with an id this firmware does not know are skipped.
 *
 * Building a list on the stack and reading it back:
 *
 * @code{.c}
 * AttributeList list = {};
 * attribute_list_add_cstring(&list, AttributeIdTitle, "Lunch");
 * attribute_list_add_cstring(&list, AttributeIdLocationName, "Cafe");
 * attribute_list_add_resource_id(&list, AttributeIdIconPin, TIMELINE_RESOURCE_TIMELINE_CALENDAR);
 * attribute_list_add_uint8(&list, AttributeIdBgColor, GColorOrangeARGB8);
 *
 * for (int i = 0; i < list.num_attributes; i++) {
 *   const Attribute *attr = &list.attributes[i];
 *   switch (attr->id) {
 *     case AttributeIdTitle:
 *     case AttributeIdLocationName:
 *       PBL_LOG_DBG("%d: %s", attr->id, attr->cstring);
 *       break;
 *     case AttributeIdIconPin:
 *       PBL_LOG_DBG("icon: %" PRIu32, attr->uint32);
 *       break;
 *     default:
 *       break;
 *   }
 * }
 *
 * const char *location = attribute_get_string(&list, AttributeIdLocationName, "");
 *
 * attribute_list_destroy_list(&list);
 * @endcode
 * @{
 */

/** @brief Size of large icons, in pixels. */
#define ATTRIBUTE_ICON_LARGE_SIZE_PX 80
/** @brief Size of small icons, in pixels. */
#define ATTRIBUTE_ICON_SMALL_SIZE_PX 50
/** @brief Size of tiny icons, in pixels. */
#define ATTRIBUTE_ICON_TINY_SIZE_PX 25

/** @brief Maximum length of a title, in bytes. */
#define ATTRIBUTE_TITLE_MAX_LEN 64
/** @brief Maximum length of a subtitle, in bytes. */
#define ATTRIBUTE_SUBTITLE_MAX_LEN 64
/** @brief Maximum length of an app glance subtitle template string, in bytes. */
#define ATTRIBUTE_APP_GLANCE_SUBTITLE_MAX_LEN (150)

/**
 * @brief Size in bytes of a Uint32List holding a number of values.
 *
 * @param num_values Number of values.
 */
#define Uint32ListSize(num_values) (sizeof(Uint32List) + (num_values) * sizeof(uint32_t))

/** @brief Attribute identifiers, as used on the wire. */
typedef enum {
  /** Unused id; never matched by lookups. */
  AttributeIdUnused = 0,
  /** (string) Title shown in a detailed view, at most 64 bytes. */
  AttributeIdTitle = 1,
  /** (string) Auxiliary text shown under the title, at most 64 bytes. */
  AttributeIdSubtitle = 2,
  /** (string) Body text of the view, at most 512 bytes. */
  AttributeIdBody = 3,
  /** (resource id) Tiny icon, shown in the status bar or banner. */
  AttributeIdIconTiny = 4,
  /** (resource id) Small icon, shown in the rendered view. */
  AttributeIdIconSmall = 5,
  /** (resource id) Large icon, shown when actions are triggered. */
  AttributeIdIconLarge = 6,
  /** (uint8) ANCS action id persisted in the actions of ANCS notifications. */
  AttributeIdAncsAction = 7,
  /** (string list) Canned responses offered by a reply action. */
  AttributeIdCannedResponses = 8,
  /** (string) Title shown in a list view. */
  AttributeIdShortTitle = 9,
  /** (resource id) Icon shown for the pin in the timeline. */
  AttributeIdIconPin = 10,
  /** (string) Name of the location, in a broad sense, of the item. */
  AttributeIdLocationName = 11,
  /** (string) Sender of a message, organizer of an event, etc. */
  AttributeIdSender = 12,
  /** (uint32) Launch code passed to the app by an open watch app action. */
  AttributeIdLaunchCode = 13,
  /** (uint32) Unix time the item was last updated, e.g. for weather. */
  AttributeIdLastUpdated = 14,
  /** (string) Rank of the away team in the league. */
  AttributeIdRankAway = 15,
  /** (string) Rank of the home team in the league. */
  AttributeIdRankHome = 16,
  /** (string) Abbreviated name of the away team, at most 4 characters. */
  AttributeIdNameAway = 17,
  /** (string) Abbreviated name of the home team, at most 4 characters. */
  AttributeIdNameHome = 18,
  /** (string) Record (wins and losses) of the away team. */
  AttributeIdRecordAway = 19,
  /** (string) Record (wins and losses) of the home team. */
  AttributeIdRecordHome = 20,
  /** (string) Current score of the away team, in game only. */
  AttributeIdScoreAway = 21,
  /** (string) Current score of the home team, in game only. */
  AttributeIdScoreHome = 22,
  /** (uint8) GameState of a sports pin, selecting the pre-game or in-game view. */
  AttributeIdSportsGameState = 23,
  /** (string) TV channel, radio station, website, etc. broadcasting the event. */
  AttributeIdBroadcaster = 24,
  /** (string list) Section headings of a generic pin, paired with the paragraphs. */
  AttributeIdHeadings = 25,
  /** (string list) Section paragraphs of a generic pin, paired with the headings. */
  AttributeIdParagraphs = 26,
  /** (uint8) Primary color of the source, as GColor8 ARGB. */
  AttributeIdPrimaryColor = 27,
  /** (uint8) Background color of the source, as GColor8 ARGB. */
  AttributeIdBgColor = 28,
  /** (uint8) Secondary color of the source, as GColor8 ARGB. */
  AttributeIdSecondaryColor = 29,
  /** (string) Display name of the app the notification originates from. */
  AttributeIdAppName = 30,
  /** (uint8) CalendarRecurringType of a calendar event. */
  AttributeIdDisplayRecurring = 31,
  /** (string) iOS app identifier, e.g. com.apple.MobileSMS. */
  AttributeIdiOSAppIdentifier = 32,
  /** (uint8) Whether a reply action offers emoji; emoji are offered when absent. */
  AttributeIdEmojiSupported = 33,
  /** (uint32) ANCS UID of the item or action, when the parent id links to another item. */
  AttributeIdAncsId = 34,
  /** (uint8) Health insight the item comes from. */
  AttributeIdHealthInsightType = 35,
  /** (string) Subtitle shown in a list view. */
  AttributeIdShortSubtitle = 36,
  /** (uint32) ANCS timestamp of the notification, seconds since the epoch. */
  AttributeIdTimestamp = 37,
  /** (uint8) WeatherTimeType: which time, if any, a weather pin shows. */
  AttributeIdDisplayTime = 38,
  /** (string) Phone number, email address or other handle of a contact. */
  AttributeIdAddress = 39,
  /** (uint8) MuteBitfield of the days on which an app's notifications are muted. */
  AttributeIdMuteDayOfWeek = 40,
  /** (string list) Metric names of a pin. */
  AttributeIdMetricNames = 41,
  /** (string list) Metric values of a pin. */
  AttributeIdMetricValues = 42,
  /** (Uint32List) Metric icons, as TimelineResourceId. */
  AttributeIdMetricIcons = 43,
  /** (uint8) Health activity the item comes from. */
  AttributeIdHealthActivityType = 44,
  /** (uint8) AlarmKind of an alarm pin. */
  AttributeIdAlarmKind = 45,
  /** (string) Authentication code, e.g. the Nexmo SMS check-in code. */
  AttributeIdAuthCode = 46,
  /** (string) Template string shown under a title (app glances), at most 150 bytes. */
  AttributeIdSubtitleTemplateString = 47,
  /** (resource id) Generic icon. */
  AttributeIdIcon = 48,
  /** (Uint32List) Custom vibration pattern, as passed to vibes_enqueue_custom_pattern(). */
  AttributeIdVibrationPattern = 49,
  /** (uint32) Unix time at which an app's mute expires. */
  AttributeIdMuteExpiration = 50,
  /**
   * (string list) Notification filtering rules of an app, as bytes: a rule count, then for each
   * rule its match type, match field and case sensitivity bytes and a NUL-terminated pattern.
   */
  AttributeIdNotificationFilteringRules = 51,
  /**
   * (uint8) The phone holds an image for this item, fetchable over the imaging endpoint keyed by
   * the item's UUID. The value is the image's height/width in sixteenths, so the card can reserve
   * a band of the right shape before the pixels arrive. Absent or 0 means no image.
   */
  AttributeIdImageAspectRatio = 52,
  /** (uint8) WeatherPinKind of a weather pin. */
  AttributeIdWeatherPinKind = 53,
  /** Number of attribute ids. */
  NumAttributeIds,
} AttributeId;

/** @brief Variable-length list of 32-bit values. */
typedef struct {
  /** Number of values. */
  uint16_t num_values;
  /** Values. */
  uint32_t values[];
} Uint32List;

/** @brief Attribute: an id and a value whose type depends on the id. */
typedef struct {
  /** Attribute id. */
  AttributeId id;
  /** Value; the member matching the type of @ref Attribute::id is valid. */
  union {
    /** String value. */
    char *cstring;
    /** 8-bit value. */
    uint8_t uint8;
    /** 16-bit value; no attribute currently uses it. */
    uint16_t uint16;
    /** 32-bit or resource id value. */
    uint32_t uint32;
    /** Signed 8-bit value; no attribute currently uses it. */
    int8_t int8;
    /** Signed 16-bit value; no attribute currently uses it. */
    int16_t int16;
    /** Signed 32-bit value; no attribute currently uses it. */
    int32_t int32;
    /** String list value. */
    struct pbl_string_list *string_list;
    /** Uint32List value. */
    Uint32List *uint32_list;
  };
} Attribute;

/** @brief List of attributes. */
typedef struct {
  /** Number of attributes. */
  uint8_t num_attributes;
  /** Attributes. */
  Attribute *attributes;
} AttributeList;

/**
 * @brief Initialize a string attribute.
 *
 * @param[out] attribute Attribute to initialize.
 * @param buffer String the attribute points to; not copied.
 * @param attribute_id Attribute id.
 */
void attribute_init_string(Attribute *attribute, char *buffer, AttributeId attribute_id);

/**
 * @brief Deep copy an attribute, placing its string or list data in a buffer.
 *
 * @param[out] dest Destination attribute.
 * @param src Source attribute.
 * @param[in,out] buffer Pointer to free memory for the data; advanced past what was used.
 * @param buffer_end End of the buffer.
 * @return true on success, false if the buffer is too small.
 */
bool attribute_copy(Attribute *dest, const Attribute *src, uint8_t **buffer,
                    uint8_t *const buffer_end);

/**
 * @brief Deep copy an attribute list into one contiguous buffer.
 *
 * @param[out] out Destination list; its attributes and data live in @p buffer.
 * @param in Source list.
 * @param buffer Buffer of at least attribute_list_get_buffer_size() of @p in bytes.
 * @param buffer_end End of the buffer.
 * @return true on success, false if the buffer is too small.
 */
bool attribute_list_copy(AttributeList *out, const AttributeList *in, uint8_t *buffer,
                         uint8_t *const buffer_end);

/**
 * @brief Get the size of a buffer holding a deep copy of an attribute list.
 *
 * @param list Attribute list.
 * @return Size in bytes of the attributes and their data.
 */
size_t attribute_list_get_buffer_size(const AttributeList *list);

/**
 * @brief Get the size of the string and list data of an attribute list.
 *
 * @param list Attribute list.
 * @return Size in bytes, including string terminators.
 */
size_t attribute_list_get_string_buffer_size(const AttributeList *list);

/**
 * @brief Add a string attribute, or replace the value of an existing one.
 *
 * The list grows on the kernel heap. The string is not copied, so it must stay valid until the
 * list is copied or added to a timeline item.
 *
 * @param list Attribute list.
 * @param id String attribute id, e.g. Title, Subtitle or Body.
 * @param cstring String value.
 */
void attribute_list_add_cstring(AttributeList *list, AttributeId id, const char *cstring);

/**
 * @brief Add a uint32 attribute, or replace the value of an existing one.
 *
 * @param list Attribute list.
 * @param id uint32 attribute id.
 * @param uint32 Value.
 */
void attribute_list_add_uint32(AttributeList *list, AttributeId id, uint32_t uint32);

/**
 * @brief Add a resource id attribute, or replace the value of an existing one.
 *
 * @param list Attribute list.
 * @param id Icon attribute id.
 * @param resource_id Timeline resource id.
 */
void attribute_list_add_resource_id(AttributeList *list, AttributeId id, uint32_t resource_id);

/**
 * @brief Add a uint8 attribute, or replace the value of an existing one.
 *
 * @param list Attribute list.
 * @param id uint8 attribute id.
 * @param uint8 Value.
 */
void attribute_list_add_uint8(AttributeList *list, AttributeId id, uint8_t uint8);

/**
 * @brief Add a string list attribute, or replace the value of an existing one.
 *
 * The list is not copied, so it must stay valid until the attribute list is copied or added to a
 * timeline item. Asserts if @p id is not a string list attribute.
 *
 * @param list Attribute list.
 * @param id String list attribute id, e.g. Headings or Paragraphs.
 * @param string_list Value.
 */
void attribute_list_add_string_list(AttributeList *list, AttributeId id,
                                    struct pbl_string_list *string_list);

/**
 * @brief Add a Uint32List attribute, or replace the value of an existing one.
 *
 * The list is not copied, so it must stay valid until the attribute list is copied or added to a
 * timeline item. Asserts if @p id is not a Uint32List attribute.
 *
 * @param list Attribute list.
 * @param id Uint32List attribute id, e.g. MetricIcons.
 * @param uint32_list Value.
 */
void attribute_list_add_uint32_list(AttributeList *list, AttributeId id, Uint32List *uint32_list);

/**
 * @brief Add an attribute, or replace an existing one with the same id.
 *
 * The attribute is copied shallowly.
 *
 * @param list Attribute list.
 * @param new_attribute Attribute to add.
 */
void attribute_list_add_attribute(AttributeList *list, const Attribute *new_attribute);

/**
 * @brief Allocate a zeroed attribute list on the kernel heap.
 *
 * The attributes start as AttributeIdUnused; fill them in place. The attribute_list_add_ functions
 * append after them.
 *
 * @param num_attributes Number of attributes.
 * @param[out] list_out List to initialize.
 */
void attribute_list_init_list(uint8_t num_attributes, AttributeList *list_out);

/**
 * @brief Free the attributes array of a list.
 *
 * Only for lists built with attribute_list_add_ or attribute_list_init_list(), not for copies
 * made with attribute_list_copy(). Values are not freed.
 *
 * @param list Attribute list.
 */
void attribute_list_destroy_list(AttributeList *list);

/**
 * @brief Find an attribute by id.
 *
 * @param attr_list Attribute list, may be NULL.
 * @param id Attribute id.
 * @return Attribute, or NULL if not found or @p id is AttributeIdUnused.
 */
Attribute *attribute_find(const AttributeList *attr_list, AttributeId id);

/**
 * @brief Get a string attribute.
 *
 * Asserts if @p id is not a string attribute.
 *
 * @param attr_list Attribute list.
 * @param id Attribute id.
 * @param default_value Value returned when not found.
 * @return String, or @p default_value if not found.
 */
const char *attribute_get_string(const AttributeList *attr_list, AttributeId id,
                                 char *default_value);

/**
 * @brief Get a string list attribute.
 *
 * @param attr_list Attribute list.
 * @param id Attribute id.
 * @return String list, or NULL if not found.
 */
struct pbl_string_list *attribute_get_string_list(const AttributeList *attr_list, AttributeId id);

/**
 * @brief Get a uint8 attribute.
 *
 * @param attr_list Attribute list.
 * @param id Attribute id.
 * @param default_value Value returned when not found.
 * @return Value, or @p default_value if not found.
 */
uint8_t attribute_get_uint8(const AttributeList *attr_list, AttributeId id, uint8_t default_value);

/**
 * @brief Get a uint32 or resource id attribute.
 *
 * @param attr_list Attribute list.
 * @param id Attribute id.
 * @param default_value Value returned when not found.
 * @return Value, or @p default_value if not found.
 */
uint32_t attribute_get_uint32(const AttributeList *attr_list, AttributeId id,
                              uint32_t default_value);

/**
 * @brief Get a Uint32List attribute.
 *
 * Asserts if @p id is not a Uint32List attribute.
 *
 * @param attr_list Attribute list.
 * @param id Attribute id.
 * @return List, or NULL if not found.
 */
Uint32List *attribute_get_uint32_list(const AttributeList *attr_list, AttributeId id);

/**
 * @brief Serialize an attribute list.
 *
 * @param attr_list Attribute list.
 * @param[out] buffer Output buffer of at least attribute_list_get_serialized_size() bytes.
 * @param buf_end End of the buffer.
 * @return Number of bytes written.
 */
size_t attribute_list_serialize(const AttributeList *attr_list, uint8_t *buffer, uint8_t *buf_end);

/**
 * @brief Get the serialized size of an attribute list.
 *
 * @param attr_list Attribute list, may be NULL.
 * @return Size in bytes.
 */
size_t attribute_list_get_serialized_size(const AttributeList *attr_list);

/**
 * @brief Check that a serialized attribute list is well formed and record which ids it contains.
 *
 * @param cursor Start of the serialized attributes.
 * @param val_end End of the data.
 * @param num_attributes Number of serialized attributes.
 * @param[out] has_attribute Array of NumAttributeIds flags, indexed by id; set for each known id
 *                           found.
 * @return true if well formed.
 */
bool attribute_check_serialized_list(const uint8_t *cursor, const uint8_t *val_end,
                                     uint8_t num_attributes, bool has_attribute[]);

/**
 * @brief Get the size of the buffer needed to deserialize attributes.
 *
 * @param num_attributes Number of serialized attributes.
 * @param[in,out] cursor Start of the serialized attributes; advanced past them.
 * @param end End of the data.
 * @return Size in bytes of the string and list data, or a negative value if the data is
 *         malformed.
 */
int32_t attribute_get_buffer_size_for_serialized_attributes(uint8_t num_attributes,
                                                            const uint8_t **cursor,
                                                            const uint8_t *end);

/**
 * @brief Deserialize attributes into an attribute list.
 *
 * Attributes with an id this firmware does not know are skipped, and
 * @c attr_list->num_attributes is updated to the number actually stored.
 *
 * @param[in,out] buffer Buffer for string and list data; advanced past what was used.
 * @param buf_end End of the buffer.
 * @param[in,out] cursor Start of the serialized attributes; advanced past them.
 * @param payload_end End of the serialized data.
 * @param[in,out] attr_list List with @c num_attributes set to the serialized count and room for
 *                          that many attributes.
 * @return true on success, false if the data is malformed.
 */
bool attribute_deserialize_list(char **buffer, char *const buf_end, const uint8_t **cursor,
                                const uint8_t *payload_end, AttributeList *attr_list);

/** @} */
