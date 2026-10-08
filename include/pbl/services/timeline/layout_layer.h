/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "attribute.h"

#include <pbl/util/uuid.h>

#include <applib/graphics/gtypes.h>
#include <applib/ui/animation.h>
#include <applib/ui/layer.h>

/**
 * @defgroup services_timeline_layout_layer Layout layers
 * @ingroup services_timeline
 * @brief Layers that render templated content such as timeline items.
 *
 * A LayoutLayer displays a TimelineItem (pin, reminder or notification) from its attributes. It
 * differs from a plain Layer in that:
 *
 * - it is modulated by a LayoutLayerMode, the context it is shown in, e.g. a detailed card or a
 *   compact pin in the Timeline list;
 * - it exposes generic methods: layout_create(), layout_destroy() and layout_get_size();
 * - it is built from the set of attributes it displays;
 * - sub-types are instantiated by LayoutId rather than through specialized constructors.
 *
 * @code{.c}
 * TimelineLayoutInfo info;
 * timeline_layout_init_info(&info, item, time_util_get_midnight_of(now));
 * const LayoutLayerConfig config = {
 *   .frame = &frame,
 *   .attributes = &item->attr_list,
 *   .mode = LayoutLayerModeCard,
 *   .app_id = &item->header.parent_id,
 *   .context = &info,
 * };
 * LayoutLayer *layout = layout_create(item->header.layout, &config);
 * layer_add_child(parent, (Layer *)layout);
 * ...
 * layout_destroy(layout);
 * @endcode
 * @{
 */

/** @brief Direction in which text is anchored. */
typedef enum {
  /** Scrolling up, past mode. */
  LayoutLayerAnchorTextDirectionUp,
  /** Scrolling down, future mode. */
  LayoutLayerAnchorTextDirectionDown,
} LayoutLayerAnchorTextDirection;

/**
 * @brief Type of a LayoutLayer, which tells which attributes to expect.
 *
 * Stored in CommonTimelineItemHeader::layout.
 */
typedef enum {
  /** Not a valid id. */
  LayoutIdUnknown = 0,
  /** Generic pin; requires a title. */
  LayoutIdGeneric,
  /** Calendar pin; requires a title. */
  LayoutIdCalendar,
  /** Reminder, rendered by the notification layout; requires a title. */
  LayoutIdReminder,
  /** Notification; requires a title. */
  LayoutIdNotification,
  /** Communication notification; not implemented, never verifies. */
  LayoutIdCommNotification,
  /** Weather pin; requires a title and a location name. */
  LayoutIdWeather,
  /** Sports pin; requires a title. */
  LayoutIdSports,
  /** Alarm pin; requires a title and a subtitle. */
  LayoutIdAlarm,
  /** Health pin; requires a title. */
  LayoutIdHealth,
  /** Number of layout ids. */
  NumLayoutIds,
  /** Unit test layout with no attribute requirements, rendered as generic. */
  LayoutIdTest,
} LayoutId;

/** @brief Colors of a layout. */
typedef struct {
  /** Primary (foreground) color. */
  GColor primary_color;
  /** Secondary color. */
  GColor secondary_color;
  /** Background color. */
  GColor bg_color;
} LayoutColors;

/** @brief Context in which a layout is displayed. */
typedef enum {
  /** No mode. */
  LayoutLayerModeNone = 0,
  /** Overlay shown like a partially obstructing HUD (Timeline Peek). */
  LayoutLayerModePeek,
  /** Menu-style mode in the Timeline app, fat first item. */
  LayoutLayerModePinnedFat,
  /** Menu-style mode in the Timeline app, thin second item. */
  LayoutLayerModePinnedThin,
  /** Card showing the details of a TimelineItem. */
  LayoutLayerModeCard,
  /** Number of modes. */
  NumLayoutLayerModes,
} LayoutLayerMode;

struct LayoutLayer;

/** @brief Configuration of a new LayoutLayer. */
typedef struct LayoutLayerConfig LayoutLayerConfig;

/**
 * @brief Destructor of a LayoutLayer.
 *
 * @param layout Layout to destroy.
 */
typedef void (*LayoutLayerDestructor)(struct LayoutLayer *layout);

/**
 * @brief Constructor of a LayoutLayer.
 *
 * @param config Configuration; each layout type expects specific attributes and context.
 * @return New layout, allocated on the calling task's heap.
 */
typedef struct LayoutLayer *(*LayoutLayerConstructor)(const LayoutLayerConfig *config);

/**
 * @brief Check whether attributes satisfy the requirements of a layout.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if the required attributes are present.
 */
typedef bool (*LayoutVerifier)(bool existing_attributes[]);

#pragma push_macro("GSize")
#undef GSize // [FBO] ugly work around for rogue macro
/**
 * @brief Get the size of a layout's content, set by its text and icons.
 *
 * @param ctx Graphics context the layout is rendered in.
 * @param layout Layout.
 * @return Size of the content.
 */
typedef struct GSize (*LayoutLayerSizeGetter)(GContext *ctx, struct LayoutLayer *layout);
#pragma pop_macro("GSize")

/**
 * @brief Change the mode of a layout.
 *
 * @param layout Layout.
 * @param final_mode New mode.
 */
typedef void (*LayerLayerModeSetter)(struct LayoutLayer *layout, LayoutLayerMode final_mode);

#if PBL_COLOR
/**
 * @brief Get the colors of a layout.
 *
 * @param layout Layout.
 * @return Colors.
 */
typedef const LayoutColors *(*LayoutLayerColorsGetter)(const struct LayoutLayer *layout);
#endif

/**
 * @brief Get the type-specific context of a layout.
 *
 * @param layout Layout.
 * @return Context; the notification layout returns its TimelineItem.
 */
typedef void *(*LayoutLayerContextGetter)(struct LayoutLayer *layout);

/** @brief Methods of a LayoutLayer type. */
typedef struct {
  /** Content size getter. */
  LayoutLayerSizeGetter size_getter;
  /** Destructor. */
  LayoutLayerDestructor destructor;
  /** Mode setter. */
  LayerLayerModeSetter mode_setter;
#if PBL_COLOR
  /** Colors getter, may be NULL for the default colors. */
  LayoutLayerColorsGetter color_getter;
#endif
  /** Context getter, may be NULL. */
  LayoutLayerContextGetter context_getter;
} LayoutLayerImpl;

/** @brief Base of all layouts; can be cast to a Layer. */
typedef struct LayoutLayer {
  /** Underlying layer. */
  Layer layer;
  /** Current mode. */
  LayoutLayerMode mode;
  /** Attributes displayed; not owned. */
  AttributeList *attributes;
  /** Methods of the layout type. */
  const LayoutLayerImpl *impl;
} LayoutLayer;

/** @brief Configuration of a new LayoutLayer. */
struct LayoutLayerConfig {
  /** Frame of the layer. */
  const GRect *frame;
  /** Attributes to display; must outlive the layout. */
  AttributeList *attributes;
  /** Initial mode. */
  LayoutLayerMode mode;
  /** App owning the item, used to resolve app-published icons. */
  const Uuid *app_id;
  /**
   * Type-specific context: a TimelineLayoutInfo for timeline layouts, a NotificationLayoutInfo
   * for the notification and reminder layouts.
   */
  void *context;
};

/**
 * @brief Create a layout of a type.
 *
 * @param id Layout type; LayoutIdTest creates a generic layout. Must not be LayoutIdUnknown.
 * @param config Configuration.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *layout_create(LayoutId id, const LayoutLayerConfig *config);

/**
 * @brief Check whether attributes satisfy the requirements of a layout type.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @param id Layout type.
 * @return true if valid; always true for LayoutIdTest, false for unknown or unimplemented types.
 */
bool layout_verify(bool existing_attributes[], LayoutId id);

/**
 * @brief Get the size of a layout's content.
 *
 * @param ctx Graphics context.
 * @param layout Layout.
 * @return Size of the content.
 */
GSize layout_get_size(GContext *ctx, LayoutLayer *layout);

/**
 * @brief Get the colors of a layout.
 *
 * @param layout Layout.
 * @return Layout colors, or defaults on black and white platforms or if the type has none.
 */
const LayoutColors *layout_get_colors(const LayoutLayer *layout);

/**
 * @brief Get the colors of a notification layout.
 *
 * On black and white platforms the colors follow the notification design preference.
 *
 * @param layout Layout.
 * @return Colors.
 */
const LayoutColors *layout_get_notification_colors(const LayoutLayer *layout);

/**
 * @brief Change the mode of a layout.
 *
 * @param layout Layout.
 * @param final_mode New mode.
 */
void layout_set_mode(LayoutLayer *layout, LayoutLayerMode final_mode);

/**
 * @brief Destroy a layout with its type's destructor.
 *
 * @param layout Layout.
 */
void layout_destroy(LayoutLayer *layout);

/**
 * @brief Get the type-specific context of a layout.
 *
 * @param layout Layout.
 * @return Context, or NULL if the type has none.
 */
void *layout_get_context(LayoutLayer *layout);

/** @} */
