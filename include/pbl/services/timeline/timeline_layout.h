/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "layout_layer.h"
#include "layout_node.h"

// TODO: PBL-28902 Timeline card layouts integration tests

#include <pbl/services/clock.h>
#include <pbl/services/timeline/timeline.h>
#include <pbl/services/timeline/timeline_resources.h>

#include <applib/graphics/gpath.h>
#include <applib/graphics/gtypes.h>
#include <applib/ui/kino/kino_layer.h>
#include <applib/ui/status_bar_layer.h>
#include <apps/system/timeline/text_node.h>

/**
 * @defgroup services_timeline_timeline_layout Timeline layouts
 * @ingroup services_timeline
 * @brief Common base of the pin layouts (generic, calendar, weather, sports, alarm, health).
 *
 * A TimelineLayout renders a pin either as a compact pin view (peek and Timeline list modes),
 * built here from the primary and secondary attributes of its TimelineLayoutImpl, or as a card,
 * built by the implementation's card view constructor. It owns the pin icon, chosen from the icon
 * attributes for the mode. The LayoutLayerConfig context must be a TimelineLayoutInfo.
 * @{
 */

/** @brief Maximum height of a card view, in pixels. */
#define TIMELINE_MAX_BOX_HEIGHT 2500
/** @brief Top margin of the pin view, in pixels. */
#define TIMELINE_TOP_MARGIN 10

/** @brief Height of the page break arrow of a card, in pixels. */
#define TIMELINE_CARD_ARROW_HEIGHT 13
/** @brief Horizontal margin of a card, in pixels. */
#define TIMELINE_CARD_MARGIN PBL_IF_RECT_ELSE(7, 12)
/** @brief Duration of the pin to card transition, in milliseconds. */
#define TIMELINE_CARD_TRANSITION_MS (interpolate_moook_duration())
/** @brief Bottom margin of a card body section header, in pixels. */
#define TIMELINE_CARD_BODY_HEADER_MARGIN_HEIGHT                                                 \
  PREFERRED_CONTENT_SIZE_SWITCH(                                                                \
      PreferredContentSizeDefault,     /* This is the same as Medium until Small is designed */ \
      /* small */ -2, /* medium */ -2, /* large */                                              \
      2, /* This is the same as Large until ExtraLarge is designed */ /* extralarge */ 2)
/** @brief Bottom margin of a card body section, in pixels. */
#define TIMELINE_CARD_BODY_MARGIN_HEIGHT                                                        \
  PREFERRED_CONTENT_SIZE_SWITCH(                                                                \
      PreferredContentSizeDefault,     /* This is the same as Medium until Small is designed */ \
      /* small */ 17, /* medium */ 17, /* large */                                              \
      15, /* This is the same as Large until ExtraLarge is designed */ /* extralarge */ 15)

/** @brief Layout rendering a pin. */
typedef struct TimelineLayout TimelineLayout;

/** @brief Implementation of a TimelineLayout type. */
typedef struct TimelineLayoutImpl TimelineLayoutImpl;

/**
 * @brief Build the card view of a layout.
 *
 * @param layout Layout.
 * @return Root node of the card view.
 */
typedef GTextNode *(*TimelineLayoutViewConstructor)(TimelineLayout *layout);

/**
 * @brief Release resources of a card view before its nodes are destroyed.
 *
 * @param layout Layout.
 */
typedef void (*TimelineLayoutViewDeinitializer)(TimelineLayout *layout);

/**
 * @brief Get the primary text of a layout.
 *
 * @param layout Layout.
 * @return Text, or NULL to use the primary attribute.
 */
typedef const char *(*TimelineLayoutPrimaryTextGetter)(const TimelineLayout *layout);

/** @brief Scroll direction of the Timeline. */
typedef enum {
  /** Timeline past. */
  TimelineScrollDirectionUp,
  /** Timeline future. */
  TimelineScrollDirectionDown,
} TimelineScrollDirection;

/** @brief Timing information of the pin rendered by a TimelineLayout. */
typedef struct {
  /** App owning the pin. */
  Uuid app_id;
  /** Start time of the pin. */
  time_t timestamp;
  /** Midnight of the day the pin is shown in. */
  time_t current_day;
  /** End time of the pin. */
  time_t end_time;
  /** Time shown on the pin: the end time on the last day of a multi-day event, else the start. */
  time_t pin_time;
  /** Duration in seconds. */
  uint32_t duration_s;
  /** Timeline direction the pin is shown in. */
  TimelineScrollDirection scroll_direction;
  /** The pin is all day, or spans the whole current day. */
  bool all_day;
  /**
   * Number of concurrent events besides the first: 0 for a single event, 1 for two overlapping
   * events. Only valid for Timeline Peek.
   */
  unsigned int num_concurrent;
} TimelineLayoutInfo;

/** @brief Layout rendering a pin; can be cast to a LayoutLayer. */
struct TimelineLayout {
  /** Base layout. */
  LayoutLayer layout_layer;
  /** Colors, from the color attributes or the implementation's defaults. */
  LayoutColors colors;

  /** Pin icon. */
  KinoLayer icon_layer;
  /** Size of the pin icon. */
  GSize icon_size;
  /** Resource of the pin icon. */
  AppResourceInfo icon_res_info;
  /** Resource id of the pin icon. */
  uint32_t icon_resource_id;
  /** Height of the content before the page break. */
  int16_t page_break_height;
  /** The card has a page break, enabling the special first scroll behavior. */
  bool has_page_break;

  /** Implementation. */
  const TimelineLayoutImpl *impl;
  /** Timing of the pin, from the LayoutLayerConfig context. */
  TimelineLayoutInfo *info;

  /** Timeline resource of the icon. */
  TimelineResourceInfo icon_info;

  /** Root of the view. */
  GTextNode *view_node;
  /** Size of the view. */
  GSize view_size;

  /** Layout this one is transitioning to. */
  struct TimelineLayout *transition_layout;
  /** Transition animation, unscheduled on deinit. */
  Animation *transition_animation;

  /** Icon layers of the metrics. */
  KinoLayer **metric_icon_layers;
  /** Number of metric icon layers. */
  unsigned int num_metric_icon_layers;

  /** Set while being destroyed, so animation stopped handlers do not start more animations. */
  bool is_being_destroyed;
};

/** @brief Implementation of a TimelineLayout type. */
struct TimelineLayoutImpl {
  /** Attributes shown in the pin view. */
  struct {
    /** Primary text attribute. */
    AttributeId primary_id;
    /** Secondary text attribute. */
    AttributeId secondary_id;
  } attributes;

  /** Colors used when the item has no color attributes. */
  LayoutColors default_colors;

  /** Icon used when the item has none. */
  TimelineResourceId default_icon;
  /** Alignment of the icon in the card. */
  GAlign card_icon_align;
  /** Size of the icon in the card. */
  TimelineResourceSize card_icon_size;

  /** Builds the card view. */
  TimelineLayoutViewConstructor card_view_constructor;
  /** Releases card view resources, may be NULL. */
  TimelineLayoutViewDeinitializer card_view_deinitializer;
  /** Optional, overrides the primary text of the pin and peek views when it returns non-NULL. */
  TimelineLayoutPrimaryTextGetter get_primary_text;
};

/**
 * @brief Pick the icon of a layout from its attributes.
 *
 * Cards try the icon attribute of @p icon_size first, then the tiny icon, then the pin icon;
 * other modes try the pin icon, then the tiny icon, then the icon of @p icon_size.
 *
 * @param mode Layout mode.
 * @param attributes Attributes of the item.
 * @param icon_size Size of the card icon.
 * @param fallback_resource Icon used if no attribute is set.
 * @return Timeline resource id.
 */
TimelineResourceId timeline_layout_get_icon_resource_id(LayoutLayerMode mode,
                                                        const AttributeList *attributes,
                                                        TimelineResourceSize icon_size,
                                                        TimelineResourceId fallback_resource);

/**
 * @brief Initialize a TimelineLayout, picking its icon from the attributes.
 *
 * @param[out] layout Layout to initialize.
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @param timeline_layout_impl Implementation.
 */
void timeline_layout_init(TimelineLayout *layout, const LayoutLayerConfig *config,
                          const TimelineLayoutImpl *timeline_layout_impl);

/**
 * @brief Initialize a TimelineLayout with a given icon.
 *
 * @param[out] layout Layout to initialize.
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @param timeline_layout_impl Implementation.
 * @param icon_resource Icon; the implementation's default icon is used if it cannot be loaded.
 */
void timeline_layout_init_with_icon_id(TimelineLayout *layout, const LayoutLayerConfig *config,
                                       const TimelineLayoutImpl *timeline_layout_impl,
                                       TimelineResourceId icon_resource);

/**
 * @brief Fill the timing information of a pin.
 *
 * Marks the pin all day, in @p info and in @p item, if it spans @p current_day.
 *
 * @param[out] info Timing information.
 * @param[in,out] item Pin.
 * @param current_day Midnight of the day the pin is shown in.
 */
void timeline_layout_init_info(TimelineLayoutInfo *info, TimelineItem *item, time_t current_day);

/**
 * @brief Release the resources of a TimelineLayout without freeing it.
 *
 * @param timeline_layout Layout.
 */
void timeline_layout_deinit(TimelineLayout *timeline_layout);

/**
 * @brief Get the frame of the pin icon.
 *
 * @param bounds Bounds of the layout.
 * @param scroll_direction Timeline direction.
 * @param[out] frame Icon frame.
 */
void timeline_layout_get_icon_frame(const GRect *bounds, TimelineScrollDirection scroll_direction,
                                    GRect *frame);

/**
 * @brief LayoutLayerSizeGetter of timeline layouts.
 *
 * @param ctx Graphics context.
 * @param layout Layout.
 * @return Size of the view.
 */
GSize timeline_layout_get_content_size(GContext *ctx, LayoutLayer *layout);

/**
 * @brief LayoutLayerDestructor of timeline layouts; deinitializes and frees the layout.
 *
 * @param layout Layout.
 */
void timeline_layout_destroy(LayoutLayer *layout);

/**
 * @brief Mode setter of timeline layouts; rebuilds the view for the new mode.
 *
 * @param layout Layout.
 * @param final_mode New mode.
 */
void timeline_layout_change_mode(LayoutLayer *layout, LayoutLayerMode final_mode);

/**
 * @brief Colors getter of timeline layouts.
 *
 * @param layout Layout.
 * @return Colors of the layout.
 */
const LayoutColors *timeline_layout_get_colors(const LayoutLayer *layout);

/**
 * @brief Build the view of a layout for a mode and measure it.
 *
 * @param layout Layout.
 * @param mode Mode to build the view for.
 */
void timeline_layout_init_view(TimelineLayout *layout, LayoutLayerMode mode);

/**
 * @brief Destroy the view of a layout.
 *
 * @param layout Layout.
 */
void timeline_layout_deinit_view(TimelineLayout *layout);

/**
 * @brief Draw the view of a layout.
 *
 * @param timeline_layout Layout.
 * @param ctx Graphics context.
 */
void timeline_layout_render_view(TimelineLayout *timeline_layout, GContext *ctx);

/**
 * @brief Measure the view of a layout.
 *
 * @param timeline_layout Layout.
 * @param ctx Graphics context.
 * @param[out] size_out Size of the view.
 */
void timeline_layout_get_size(TimelineLayout *timeline_layout, GContext *ctx, GSize *size_out);

/**
 * @brief Build a card view from a vertical container config.
 *
 * Appends the headings and paragraphs and, if the item has a last updated time, a "Last updated"
 * section.
 *
 * @param layout Layout.
 * @param config Vertical container config.
 * @return Root node of the card view.
 */
GTextNode *timeline_layout_create_card_view_from_config(const TimelineLayout *layout,
                                                        const LayoutNodeConfig *config);

/**
 * @brief Create a node positioning the layout's icon.
 *
 * @param layout Layout.
 * @return New node.
 */
GTextNodeCustom *timeline_layout_create_icon_node(const TimelineLayout *layout);

/**
 * @brief Create a page break node, which draws the glance arrow.
 *
 * @param layout Layout.
 * @return New node.
 */
GTextNodeCustom *timeline_layout_create_page_break_node(const TimelineLayout *layout);

/**
 * @brief LayoutNodeTextDynamicUpdate writing the start time of the pin.
 *
 * @param layout Layout, a TimelineLayout.
 * @param config Config of the node.
 * @param[out] buffer Text buffer.
 * @param render true when drawing.
 */
void timeline_layout_time_text_update(const LayoutLayer *layout,
                                      const LayoutNodeTextDynamicConfig *config, char *buffer,
                                      bool render);

/** @} */
