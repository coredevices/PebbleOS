/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"
#include "layout_layer.h"
#include "timeline_layout.h"

#include <pbl/services/timeline/attribute.h>
#include <pbl/services/timeline/timeline_resources.h>

#include <applib/ui/kino/kino_layer.h>

/**
 * @defgroup services_timeline_notification_layout Notification layout
 * @ingroup services_timeline
 * @brief Layout of notifications and reminders (LayoutIdNotification, LayoutIdReminder).
 *
 * Only the card mode is rendered. The card shows a banner with the tiny icon and app name, then
 * the title, subtitle, location, body (or a large emoji when the body is a single Jumboji, see
 * @ref services_timeline_notification_jumboji_table), an optional phone-supplied image and the
 * timestamp. The LayoutLayerConfig context must be a NotificationLayoutInfo.
 * @{
 */

/**
 * @brief Banner height of notification and reminder layouts on rectangular displays, excluding
 * the status bar; the top and bottom banners have the same size.
 */
#define LAYOUT_BANNER_HEIGHT_RECT                                                               \
  PREFERRED_CONTENT_SIZE_SWITCH(                                                                \
      PreferredContentSizeDefault,     /* This is the same as Medium until Small is designed */ \
      /* small */ 36, /* medium */ 36, /* large */                                              \
      29, /* This is the same as Large until ExtraLarge is designed */ /* extralarge */ 29)
/** @brief Height of the top banner on round displays, including the status bar. */
#define LAYOUT_TOP_BANNER_HEIGHT_ROUND 60
/** @brief Height of the bottom banner on round displays. */
#define LAYOUT_BOTTOM_BANNER_HEIGHT_ROUND 40
/** @brief Height of the top banner, excluding the status bar. */
#define LAYOUT_TOP_BANNER_HEIGHT              \
  PBL_IF_RECT_ELSE(LAYOUT_BANNER_HEIGHT_RECT, \
                   LAYOUT_TOP_BANNER_HEIGHT_ROUND - STATUS_BAR_LAYER_HEIGHT)
/** @brief Height of the white arrow layer. */
#define LAYOUT_ARROW_HEIGHT PBL_IF_RECT_ELSE(19, 16)
/** @brief Height of the layout text region: the display minus the status bar and arrow. */
#define LAYOUT_HEIGHT (DISP_ROWS - STATUS_BAR_LAYER_HEIGHT - LAYOUT_ARROW_HEIGHT)
/** @brief Vertical offset before the text begins. */
#define LAYOUT_BEFORE_TEXT_SPACING_Y 40
/** @brief Radius of the round banner. */
#define BANNER_CIRCLE_RADIUS 140
/** @brief Vertical origin of the round top banner circle. */
#define LAYOUT_TOP_BANNER_ORIGIN_Y \
  (LAYOUT_TOP_BANNER_HEIGHT_ROUND - (BANNER_CIRCLE_RADIUS + STATUS_BAR_LAYER_HEIGHT) - 1)

/** @brief Largest layout origin y at which its text is considered visible. */
#define TEXT_VISIBLE_UPPER_THRESHOLD (STATUS_BAR_LAYER_HEIGHT - INTERPOLATE_MOOOK_BOUNCE_BACK - 1)
/**
 * @brief Smallest layout origin y at which its text is considered visible.
 *
 * @param h Height of the layout.
 */
#define TEXT_VISIBLE_LOWER_THRESHOLD(h) \
  (-(h) + DISP_ROWS - STATUS_BAR_LAYER_HEIGHT - INTERPOLATE_MOOOK_BOUNCE_BACK - 1)

/**
 * @brief Fonts and spacing of a notification card, per content size.
 *
 * Offset and margin follow GTextNode: an offset moves an element without affecting its
 * neighbors, a margin changes its size without affecting its internal positioning. Padding is
 * extra space between an element and the next one below; upper padding is the same towards the
 * element above. New fields should be offsets or margins rather than paddings. All values are in
 * pixels.
 */
typedef struct {
  /** Font key of the header. */
  const char *header_font_key;
  /** Font key of the title. */
  const char *title_font_key;
  /** Font key of the subtitle. */
  const char *subtitle_font_key;
  /** Font key of the body. */
  const char *body_font_key;
  /** Font key of the footer. */
  const char *footer_font_key;
  /** Padding of the header (app name). */
  int8_t header_padding;
  /** Additional title offset when there is a body icon. */
  int8_t title_offset_if_body_icon;
  /** Padding of the title. */
  int8_t title_padding;
  /** Line spacing delta of the title. */
  int8_t title_line_delta;
  /** Upper padding of the subtitle. */
  int8_t subtitle_upper_padding;
  /** Lower padding of the subtitle. */
  int8_t subtitle_lower_padding;
  /** Line spacing delta of the subtitle. */
  int8_t subtitle_line_delta;
  /** Offset of the location. */
  int8_t location_offset;
  /** Margin of the location. */
  int8_t location_margin;
  /** Padding of the body. */
  int8_t body_padding;
  /** Line spacing delta of the body. */
  int8_t body_line_delta;
  /** Offset of the large body icon, currently used by Jumboji. */
  int8_t body_icon_offset;
  /** Margin of the large body icon. */
  int8_t body_icon_margin;
  /** Upper padding of the timestamp. */
  int8_t timestamp_upper_padding;
  /** Lower padding of the timestamp. */
  int8_t timestamp_lower_padding;
} NotificationStyle;

/** @brief LayoutLayerConfig context of a notification layout. */
typedef struct {
  /** Notification or reminder shown; must outlive the layout. */
  TimelineItem *item;
  /** Whether to show how long ago the notification arrived. */
  bool show_notification_timestamp;
} NotificationLayoutInfo;

/** @brief Notification and reminder layout; can be cast to a LayoutLayer. */
typedef struct {
  /** Base layout. */
  LayoutLayer layout;
  /** Banner icon. */
  KinoLayer icon_layer;
  /** Resource of the banner icon. */
  AppResourceInfo icon_res_info;
  /** Colors. */
  LayoutColors colors;
  /** Item shown, copied from the config context. */
  NotificationLayoutInfo info;
  /** Large body icon; allocated separately since it is rarely used. */
  KinoLayer *detail_icon_layer;
  /** Style of the current content size. */
  const NotificationStyle *style;
  /** Root of the card view. */
  GTextNode *view_node;
  /** Size of the card view. */
  GSize view_size;
} NotificationLayout;

/** @brief Default notification background color. */
#define DEFAULT_NOTIFICATION_COLOR (GColorFolly)
/** @brief Default reminder background color. */
#define DEFAULT_REMINDER_COLOR (GColorRed)
/** @brief Icon of notifications without one. */
static const TimelineResourceId NOTIF_FALLBACK_ICON = TIMELINE_RESOURCE_NOTIFICATION_GENERIC;
/** @brief Icon of reminders without one. */
static const TimelineResourceId REMINDER_FALLBACK_ICON = TIMELINE_RESOURCE_NOTIFICATION_REMINDER;
/** @brief Height of the tiny icon in the notification banner. */
#define NOTIFICATION_TINY_RESOURCE_HEIGHT (ATTRIBUTE_ICON_TINY_SIZE_PX)
/** @brief Size of the tiny icon in the notification banner; some icons are 30 px wide. */
#define NOTIFICATION_TINY_RESOURCE_SIZE (GSize(30, NOTIFICATION_TINY_RESOURCE_HEIGHT))
/**
 * @brief Vertical adjustment of the banner icon.
 *
 * Accounts for the whitespace inside the status bar below its text, so it depends on the status
 * bar font.
 */
#define NOTIFICATION_TINY_RESOURCE_VERTICAL_OFFSET                                              \
  PREFERRED_CONTENT_SIZE_SWITCH(                                                                \
      PreferredContentSizeDefault,     /* This is the same as Medium until Small is designed */ \
      /* small */ -1, /* medium */ -1, /* large */                                              \
      -2, /* This is the same as Large until ExtraLarge is designed */ /* extralarge */ -2)
/** @brief Vertical position of the banner icon within the layout. */
#define CARD_ICON_UPPER_PADDING                                          \
  ((LAYOUT_TOP_BANNER_HEIGHT - NOTIFICATION_TINY_RESOURCE_HEIGHT) / 2) + \
      NOTIFICATION_TINY_RESOURCE_VERTICAL_OFFSET

/**
 * @brief Widest image band a notification can reserve, 4:1 landscape, as a height/width ratio in
 * sixteenths. The phone clamps to the same range.
 */
#define NOTIFICATION_IMAGE_MIN_ASPECT (4)
/** @brief Tallest image band a notification can reserve, 2:3 portrait, in sixteenths. */
#define NOTIFICATION_IMAGE_MAX_ASPECT (24)

/**
 * @brief Create a notification layout.
 *
 * @param config Configuration; its context must be a NotificationLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *notification_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of a notification or reminder.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title is present.
 */
bool notification_layout_verify(bool existing_attributes[]);

/**
 * @brief Get the size of the image band a notification layout reserves.
 *
 * The image requester and the renderer must agree on it, so it is computed here only.
 *
 * @param layout Notification layout.
 * @param[out] size_out Size of the band.
 * @return false if the item has no image.
 */
bool notification_layout_get_image_size(const LayoutLayer *layout, GSize *size_out);

/**
 * @brief Get the icon used when an item has none.
 *
 * @param item_type Item type.
 * @return @ref NOTIF_FALLBACK_ICON for notifications, @ref REMINDER_FALLBACK_ICON otherwise.
 */
TimelineResourceId notification_layout_get_fallback_icon_id(TimelineItemType item_type);

/** @} */
